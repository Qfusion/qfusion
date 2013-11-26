#!/bin/sh

MORE_OPTS=""
[ ! -z "$HOST" ] && MORE_OPTS="${MORE_OPTS} --host=${HOST}"
[ ! -z "$DATA_DIR" ] && MORE_OPTS="${MORE_OPTS} --datadir=${DATA_DIR}"
if [ "$ENABLE_SHARED" = "YES" ]; then
    MORE_OPTS="${MORE_OPTS} --enable-shared=yes"
else
    MORE_OPTS="${MORE_OPTS} --enable-shared=no"
fi

cd ${SOURCE_DIR}libsrcs/libjpeg && \
MORE_OPTS="${MORE_OPTS} --build=`./config.guess`" \
./configure --enable-static=yes \
${MORE_OPTS} && \
${MAKE}

cp -f .libs/libjpeg.a "${TARGET_DIR}libjpegstat.a"
if [ "$ENABLE_SHARED" = "YES" ]; then
	cp -f .libs/libjpeg.$SHARED_LIBRARY_EXT "${TARGET_DIR}"
fi

