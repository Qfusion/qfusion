#!/bin/sh

MORE_OPTS=""
[ ! -z "$HOST" ] && MORE_OPTS="${MORE_OPTS} --host=${HOST}"
[ ! -z "$DATA_DIR" ] && MORE_OPTS="${MORE_OPTS} --datadir=${DATA_DIR}"
[ "$ENABLE_SHARED" != "YES" ] && MORE_OPTS="${MORE_OPTS} --disable-shared"

cd ${SOURCE_DIR}libsrcs/libvorbis && \
MORE_OPTS="${MORE_OPTS} --build=`./config.guess`" \
ln -s -f ../../libogg/include/ogg ./include/ogg && \
LDFLAGS="$LDFLAGS -L`pwd`/../libogg/src/.libs/" \
./autogen.sh --disable-docs \
${MORE_OPTS} && \
${MAKE}

cp -f lib/.libs/libvorbis.a "${TARGET_DIR}libvorbisstat.a" && \
cp -f lib/.libs/libvorbisfile.a "${TARGET_DIR}libvorbisfilestat.a"

if [ "$ENABLE_SHARED" = "YES" ]; then
    cp -f lib/.libs/libvorbis.$SHARED_LIBRARY_EXT "${TARGET_DIR}" && \
    cp -f lib/.libs/libvorbisfile.$SHARED_LIBRARY_EXT "${TARGET_DIR}"
fi
