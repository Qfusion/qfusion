#!/bin/sh

MORE_OPTS=""
[ ! -z "$HOST" ] && MORE_OPTS="${MORE_OPTS} --host=${HOST}"
[ ! -z "$DATA_DIR" ] && MORE_OPTS="${MORE_OPTS} --datadir=${DATA_DIR}"
[ "$ENABLE_SHARED" = "YES" ]  && MORE_OPTS="${MORE_OPTS} --enable-shared=yes"
[ "$ENABLE_SHARED" != "YES" ] && MORE_OPTS="${MORE_OPTS} --enable-shared=no"

cd ${SOURCE_DIR}libsrcs/libpng && \
CFLAGS="$CFLAGS -I`pwd`/../zlib/" \
LDFLAGS="$LDFLAGS -L`pwd`/../zlib/lib/" \
./configure \
 ${MORE_OPTS} && \
make

cp -f .libs/libpng15.a "${TARGET_DIR}libpngstat.a"

if [ "$ENABLE_SHARED" = "YES" ]; then
	cp -f .libs/libpng15.$SHARED_LIBRARY_EXT "${TARGET_DIR}libpng.${SHARED_LIBRARY_EXT}"
fi
