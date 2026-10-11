#!/bin/sh
set -eu
tar -xf source.tar
stage="$PWD/stage"
cd source
mkdir -p "$stage/objects" "$stage/include/event2"
objects=
cc -Iport/include -I. -Iinclude -Icompat -c buffer.c -o "$stage/objects/0.o"
objects="$objects $stage/objects/0.o"
cc -Iport/include -I. -Iinclude -Icompat -c bufferevent.c -o "$stage/objects/1.o"
objects="$objects $stage/objects/1.o"
cc -Iport/include -I. -Iinclude -Icompat -c bufferevent_filter.c -o "$stage/objects/2.o"
objects="$objects $stage/objects/2.o"
cc -Iport/include -I. -Iinclude -Icompat -c bufferevent_pair.c -o "$stage/objects/3.o"
objects="$objects $stage/objects/3.o"
cc -Iport/include -I. -Iinclude -Icompat -c bufferevent_ratelim.c -o "$stage/objects/4.o"
objects="$objects $stage/objects/4.o"
cc -Iport/include -I. -Iinclude -Icompat -c bufferevent_sock.c -o "$stage/objects/5.o"
objects="$objects $stage/objects/5.o"
cc -Iport/include -I. -Iinclude -Icompat -c event.c -o "$stage/objects/6.o"
objects="$objects $stage/objects/6.o"
cc -Iport/include -I. -Iinclude -Icompat -c evmap.c -o "$stage/objects/7.o"
objects="$objects $stage/objects/7.o"
cc -Iport/include -I. -Iinclude -Icompat -c evthread.c -o "$stage/objects/8.o"
objects="$objects $stage/objects/8.o"
cc -Iport/include -I. -Iinclude -Icompat -c evutil.c -o "$stage/objects/9.o"
objects="$objects $stage/objects/9.o"
cc -Iport/include -I. -Iinclude -Icompat -c evutil_rand.c -o "$stage/objects/10.o"
objects="$objects $stage/objects/10.o"
cc -Iport/include -I. -Iinclude -Icompat -c evutil_time.c -o "$stage/objects/11.o"
objects="$objects $stage/objects/11.o"
cc -Iport/include -I. -Iinclude -Icompat -c listener.c -o "$stage/objects/12.o"
objects="$objects $stage/objects/12.o"
cc -Iport/include -I. -Iinclude -Icompat -c log.c -o "$stage/objects/13.o"
objects="$objects $stage/objects/13.o"
cc -Iport/include -I. -Iinclude -Icompat -c poll.c -o "$stage/objects/14.o"
objects="$objects $stage/objects/14.o"
cc -Iport/include -I. -Iinclude -Icompat -c signal.c -o "$stage/objects/15.o"
objects="$objects $stage/objects/15.o"
cc -Iport/include -I. -Iinclude -Icompat -c evthread_pthread.c -o "$stage/objects/16.o"
objects="$objects $stage/objects/16.o"
cc -Iport/include -I. -Iinclude -Icompat -c evdns.c -o "$stage/objects/17.o"
objects="$objects $stage/objects/17.o"
cc -Iport/include -I. -Iinclude -Icompat -c event_tagging.c -o "$stage/objects/18.o"
objects="$objects $stage/objects/18.o"
cc -Iport/include -I. -Iinclude -Icompat -c evrpc.c -o "$stage/objects/19.o"
objects="$objects $stage/objects/19.o"
cc -Iport/include -I. -Iinclude -Icompat -c http.c -o "$stage/objects/20.o"
objects="$objects $stage/objects/20.o"
ar rcs "$stage/libevent.a" $objects
cc -shared $objects -Wl,-soname,libevent.so -o "$stage/libevent.so"
cp include/event2/*.h "$stage/include/event2/"
cp include/event.h "$stage/include/"

mkdir -p "$stage/pkgconfig"
cat > "$stage/pkgconfig/libevent.pc" <<'EOF'
prefix=/usr/local
libdir=${prefix}/lib
includedir=${prefix}/include
Name: libevent
Description: Bundled libevent for wasm-posix
Version: 2.1.12
Libs: -L${libdir} -levent
Requires.private:
Cflags: -I${includedir}
EOF

mkdir -p "$DESTDIR/lib/pkgconfig" "$DESTDIR/include" "$DESTDIR/share/licenses/libevent"
cp "$stage/"*.a "$stage/"*.so "$DESTDIR/lib/"
cp -R "$stage/include/." "$DESTDIR/include/"
cp "$stage/pkgconfig/"*.pc "$DESTDIR/lib/pkgconfig/"
cp LICENSE "$DESTDIR/share/licenses/libevent/LICENSE"
