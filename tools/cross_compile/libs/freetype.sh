#!/bin/sh

MORE_OPTS=""
[ ! -z "$HOST" ] && MORE_OPTS="${MORE_OPTS} --host=${HOST}"
[ "$ENABLE_SHARED" = "YES" ]  && MORE_OPTS="${MORE_OPTS} --enable-shared=yes"
[ "$ENABLE_SHARED" != "YES" ] && MORE_OPTS="${MORE_OPTS} --enable-shared=no"

cd ${SOURCE_DIR}libsrcs/libfreetype && \
./autogen.sh && ./configure \
$MORE_OPTS && \
${MAKE}

cp objs/.libs/libfreetype.a "${TARGET_DIR}libfreetypestat.a"
if [ "$ENABLE_SHARED" = "YES" ]; then
	cp objs/.libs/libfreetype.$SHARED_LIBRARY_EXT "${TARGET_DIR}"
fi
