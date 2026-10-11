#!/bin/sh
set -eu
tar -xf source.tar
cd source
# Release includes generated parser. Avoid requiring a guest parser generator.
touch cmd-parse.c
# Upstream generic POSIX PTY implementation uses /dev/ptmx, grantpt,
# unlockpt, ptsname, TIOCSCTTY and fork; gucOS has these but no forkpty.
cp compat/forkpty-haiku.c compat/forkpty-unknown.c
# BSD spelling aliases used by upstream; gucOS provides the standard C types.
cat > gucos-types.h <<'EOF'
#ifndef GUCOS_TMUX_TYPES_H
#define GUCOS_TMUX_TYPES_H
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef char *caddr_t;
#include <errno.h>
/* Malformed imsg buffers remain errors; this core lacks EBADMSG. */
#ifndef EBADMSG
#define EBADMSG EINVAL
#endif
#include <sys/time.h>
#ifndef timerclear
#define timerclear(tv) ((tv)->tv_sec = (tv)->tv_usec = 0)
#endif
#ifndef timersub
#define timersub(a,b,r) do { (r)->tv_sec = (a)->tv_sec - (b)->tv_sec; (r)->tv_usec = (a)->tv_usec - (b)->tv_usec; if ((r)->tv_usec < 0) { --(r)->tv_sec; (r)->tv_usec += 1000000; } } while (0)
#endif
#endif
EOF
sed -i '1i#include "gucos-types.h"' compat.h
# These includes only supplied base64 declarations; upstream compat.h/base64.c
# already provide the actual implementation used by this port.
sed -i '/resolv.h/d' input.c tty-keys.c tty.c compat/base64.c
sed -i '/nameser.h/d' compat/base64.c
sed -i '1i#include "../gucos-types.h"' compat/base64.c
# Convert only K&R parameter declarations; retain upstream base64 algorithm.
awk '
/^b64_ntop\(src,/ { print "b64_ntop(const unsigned char *src, size_t srclength, char *target, size_t targsize)"; skip=1; next }
/^b64_pton\(src,/ { print "b64_pton(const char *src, unsigned char *target, size_t targsize)"; skip=1; next }
skip && /^\{/ { skip=0 }
!skip { print }
' compat/base64.c > compat/base64.c.new
mv compat/base64.c.new compat/base64.c
# These flags are absent from core and appear only in masks clearing features.
sed -i 's/|ONLRET//g; s/|ECHOCTL//g; s/ECHOKE|//g' tty.c
# Current gucOS has no disabled-control-character sentinel: all cc bytes
# are literal. Preserve that behavior rather than inventing _POSIX_VDISABLE.
sed -i 's/bspace != _POSIX_VDISABLE \&\& //g' tty-keys.c
# This port targets the documented gucOS unlimited-descriptor model. Remove
# only imsg's finite-table preflight; calloc/recvmsg errors and fd cleanup stay.
# Do not fabricate getdtablesize/getdtablecount values or a closefrom bound.
awk '
/^[[:space:]]*if \(getdtablecount\(\) \+ imsg_fd_overhead \+/ { skip=1; found++; print "\t/* gucOS: no finite descriptor quota; recvmsg reports actual failures. */"; next }
skip && /^[[:space:]]*}/ { skip=0; next }
!skip { print }
END { if (found != 1 || skip) exit 1 }
' compat/imsg.c > compat/imsg.c.new
mv compat/imsg.c.new compat/imsg.c
CC=cc AR=ar RANLIB=true YACC=true CFLAGS="-O2 --enable-fork" \
 CPPFLAGS="-I/usr/local/include -I/usr/local/include/ncurses" LDFLAGS="-L/usr/local/lib -Wl,-Bstatic" \
 sh ./configure --build=wasm32-unknown-none --host=wasm32-unknown-none \
 --prefix=/usr/local --disable-dependency-tracking --disable-utf8proc
# A real runtime closefrom is required. Never select the bounded fallback.
# Leave configure detection honest: an older core must fail this gate.
if ! grep -q '^DEFS = .* -DHAVE_CLOSEFROM=1 ' Makefile; then
 echo 'tmux requires core closefrom; descriptor-limit fallback is unsupported' >&2
 exit 1
fi
make -j1
mkdir -p "$DESTDIR/bin" "$DESTDIR/share/man/man1" "$DESTDIR/share/licenses/tmux"
cp tmux "$DESTDIR/bin/tmux"
cp tmux.1 "$DESTDIR/share/man/man1/tmux.1"
cp COPYING "$DESTDIR/share/licenses/tmux/COPYING"
# Preserve notices for bundled compatibility implementations in binary distribution.
cat compat/*.c compat/*.h > "$DESTDIR/share/licenses/tmux/compat-source-notices.txt"
