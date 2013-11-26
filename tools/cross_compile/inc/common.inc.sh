#!/bin/sh
SOURCE_DIR=$( (cd -P $(dirname $0)/../../ && pwd) )/
CFLAGS="${CFLAGS}"
MAKE="make -j `getconf _NPROCESSORS_ONLN`"
