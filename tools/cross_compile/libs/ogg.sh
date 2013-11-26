#!/bin/sh

MORE_OPTS=""
[ ! -z "$HOST" ] && MORE_OPTS="${MORE_OPTS} --host=${HOST}"
[ ! -z "$DATA_DIR" ] && MORE_OPTS="${MORE_OPTS} --datadir=${DATA_DIR}"
[ "$ENABLE_SHARED" != "YES" ] && MORE_OPTS="${MORE_OPTS} --disable-shared"

cd ${SOURCE_DIR}libsrcs/libogg && \
MORE_OPTS="${MORE_OPTS} --build=`./config.guess`" \
./configure \
${MORE_OPTS} && \
${MAKE}

cp -f src/.libs/libogg.a "${TARGET_DIR}liboggstat.a"
if [ "$ENABLE_SHARED" = "YES" ]; then
	cp -f src/.libs/libogg.so "${TARGET_DIR}"
fi