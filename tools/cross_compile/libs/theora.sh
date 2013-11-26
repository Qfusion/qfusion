#!/bin/sh

MORE_OPTS=""
[ ! -z "$HOST" ] && MORE_OPTS="${MORE_OPTS} --host=${HOST}"
[ ! -z "$DATA_DIR" ] && MORE_OPTS="${MORE_OPTS} --datadir=${DATA_DIR}"
[ "$ENABLE_SHARED" != "YES" ] && MORE_OPTS="${MORE_OPTS} --disable-shared"

cd ${SOURCE_DIR}libsrcs/libtheora && \
MORE_OPTS="${MORE_OPTS} --build=`./config.guess`" \
ln -s -f ../../libogg/include/ogg ./include/ogg && \
LDFLAGS="$LDFLAGS -L`pwd`/../libogg/src/.libs/" \
./configure --disable-encode --disable-examples \
${MORE_OPTS} && \
${MAKE}

cp -f lib/.libs/libtheora.a "${TARGET_DIR}libtheorastat.a"
if [ "$ENABLE_SHARED" = "YES" ]; then
	cp -f lib/.libs/libtheora.so "${TARGET_DIR}"
	cp -f lib/.libs/libtheoradec.so "${TARGET_DIR}"
fi