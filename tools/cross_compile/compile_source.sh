#!/bin/bash
OS=$1
ARCH=$2

COMMAND="/bin/true"
COMMAND_ENV=""

set -e

. `pwd`/inc/common.inc.sh
. `pwd`/inc/target-${OS}-${ARCH}.inc.sh

cd "$SOURCE_DIR"

cd "source"

COMMAND="cmake -G \"Unix Makefiles\""
[ ! -z "${CMAKE_TOOLCHAIN_FILE}" ] && COMMAND="${COMMAND} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
[ ! -z "${CMAKE_C_FLAGS}" ] && COMMAND="${COMMAND} -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
[ ! -z "${CMAKE_CXX_FLAGS}" ] && COMMAND="${COMMAND} -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
COMMAND="${COMMAND}; ${MAKE} clean; ${MAKE}"

echo "$COMMAND"
eval "$COMMAND"
