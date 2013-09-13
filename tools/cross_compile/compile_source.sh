#!/bin/bash
OS=$1
ARCH=$2

COMMAND="/bin/true"
COMMAND_ENV=""

set -e

. `pwd`/inc/common.inc.sh
. `pwd`/inc/host-${OS}-${ARCH}.inc.sh

TARGET_LIB_DIR="${SOURCE_DIR}/source/$OS/$ARCH/$TLIB_DIR/"

cd "$SOURCE_DIR"

cd "source"

SVN_REV=$(svnversion -c | cut -d ':' -f 2 | sed -e "s/[^0-9]//g")

echo "${COMMAND_ENV} CXXFLAGS=${CFLAGS_COMMON} ${PATH}"

CFLAGS="${CFLAGS_COMMON} ${CFLAGS_SOURCE} -I../libsrcs/zlib -I../libsrcs/libcurl/include -I../libsrcs/libjpeg -I../libsrcs/libogg/include -I../libsrcs/libvorbis/include -I../libsrcs/libtheora/include  -I../libsrcs/libfreetype/include -I${DIRECTX_DIR}include -DSVN_REV=$SVN_REV "
LDFLAGS="${LDFLAGS} -L${TARGET_LIB_DIR}"

COMMAND_PREF="${COMMAND_ENV} CFLAGS=\"${CFLAGS}\" LDFLAGS=\"${LDFLAGS}\" PATH=\"${PATH}\" "
# COMMAND="${COMMAND}; ${COMMAND_PREF} make clean"
COMMAND="${COMMAND}; ${COMMAND_PREF} make"

echo "$COMMAND"
eval "$COMMAND"
