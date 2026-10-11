#!/bin/sh
set -eu
tar -xf source.tar
stage="$DESTDIR"
work="$PWD"
src="$work/source/source/ncurses-6.5"
build="$work/ncurses-build"
mkdir -p "$build"
cd "$build"
INSTALL="/bin/sh $src/install-sh -c" CC=cc BUILD_CC=cc AR=ar RANLIB=true CFLAGS=-O2 BUILD_CFLAGS=-O2 \
 sh "$src/configure" --build=wasm32-unknown-none --prefix=/usr/local \
 --disable-widec --with-termlib --without-cxx --without-cxx-binding \
 --without-ada --without-debug --without-shared --without-tests \
 --without-manpages --disable-stripping --enable-pc-files \
 --with-pkg-config-libdir=/usr/local/lib/pkgconfig \
 --with-default-terminfo-dir=/usr/local/share/terminfo \
 --with-terminfo-dirs=/usr/local/share/terminfo \
 --with-install-prefix="$build/stage"
make -j1
make install
mkdir -p "$stage"
cp -R "$build/stage/usr/local/"* "$stage/"
# Directory symlinks are outside the package inventory format.
rm -f "$stage/lib/terminfo"
mkdir -p "$stage/share/doc/ncurses"
cp "$src/COPYING" "$stage/share/doc/ncurses/COPYING"

# Build the wide ABI separately; never overlay the narrow tools or headers.
wide="$work/ncursesw-build"
mkdir -p "$wide"
cd "$wide"
INSTALL="/bin/sh $src/install-sh -c" CC=cc BUILD_CC=cc AR=ar RANLIB=true CFLAGS=-O2 BUILD_CFLAGS=-O2 \
 sh "$src/configure" --build=wasm32-unknown-none --prefix=/usr/local \
 --enable-widec --with-termlib --disable-overwrite --without-cxx --without-cxx-binding \
 --without-ada --without-debug --without-shared --without-tests \
 --without-manpages --disable-stripping --enable-pc-files \
 --with-pkg-config-libdir=/usr/local/lib/pkgconfig \
 --with-default-terminfo-dir=/usr/local/share/terminfo \
 --with-terminfo-dirs=/usr/local/share/terminfo \
 --with-install-prefix="$wide/stage"
make -j1
make install
cp "$wide/stage/usr/local/lib/"*w.a "$stage/lib/"
cp "$wide/stage/usr/local/lib/pkgconfig/"*w.pc "$stage/lib/pkgconfig/"
cp -R "$wide/stage/usr/local/include/ncursesw" "$stage/include/"

# Current manifests describe regular files; retain alias bytes and executable modes.
find "$stage" -type l | while IFS= read -r link; do
 cp -L "$link" "$link.regular"
 rm "$link"
 mv "$link.regular" "$link"
done
mkdir -p "$stage/share/licenses/ncurses"
cp "$src/COPYING" "$stage/share/licenses/ncurses/COPYING"
