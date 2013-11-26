#!/bin/bash
OS=$1
ARCH=$2

COMMAND="/bin/true"
COMMAND_ENV=""

set -e

. `pwd`/inc/common.inc.sh
. `pwd`/inc/target-${OS}-${ARCH}.inc.sh

TARGET_LIB_DIR="${SOURCE_DIR}/source/$OS/$ARCH/$TLIB_DIR/"

cd "$SOURCE_DIR"

cd "source"

echo "${COMMAND_ENV} CXXFLAGS=${CFLAGS_COMMON} ${PATH}"

CFLAGS="${CFLAGS_COMMON} ${CFLAGS_SOURCE} -I../libsrcs/zlib -I../libsrcs/libcurl/include -I../libsrcs/libjpeg -I../libsrcs/libogg/include -I../libsrcs/libvorbis/include -I../libsrcs/libtheora/include -I../libsrcs/libfreetype/include -I../libsrcs/libpng "
LDFLAGS="${LDFLAGS} -L${TARGET_LIB_DIR}"

COMMAND_PREF="${COMMAND_ENV} CFLAGS=\"${CFLAGS}\" LDFLAGS=\"${LDFLAGS}\" PATH=\"${PATH}\" "
COMMAND="${COMMAND}; ${COMMAND_PREF} ${MAKE} clean"
COMMAND="${COMMAND}; ${COMMAND_PREF} ${MAKE}"

echo "$COMMAND"
eval "$COMMAND"
