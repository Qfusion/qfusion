#!/bin/sh

MORE_OPTS=""
[ ! -z "$HOST" ] && MORE_OPTS="${MORE_OPTS} --host=${HOST}"

cd ${SOURCE_DIR}libsrcs/libfreetype && \
./autogen.sh && ./configure \
$MORE_OPTS && \
make

cp objs/.libs/libfreetype.a "${TARGET_DIR}"
if [ "$ENABLE_SHARED" = "YES" ]; then
	cp objs/.libs/libfreetype.$SHARED_LIBRARY_EXT "${TARGET_DIR}"
fi
