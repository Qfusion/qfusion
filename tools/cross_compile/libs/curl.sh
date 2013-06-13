#!/bin/sh

MORE_OPTS=""
[ ! -z "$HOST" ] && MORE_OPTS="${MORE_OPTS} --host=${HOST}"
[ ! -z "$DATA_DIR" ] && MORE_OPTS="${MORE_OPTS} --datadir=${DATA_DIR}"
[ "$ENABLE_SHARED" = "YES" ] && MORE_OPTS="${MORE_OPTS} --enable-shared"

cd ${SOURCE_DIR}libsrcs/libcurl && \
./configure --with-zlib=`pwd`/../zlib/ \
 --enable-static --disable-ldap --disable-ldaps --disable-dict --disable-telet \
 --disable-tftp --disable-manual --disable-file --without-ssl --without-libidn --enable-ipv6 \
 ${MORE_OPTS} && \
make

cp -f lib/.libs/libcurl.a "${TARGET_DIR}libcurlstat.a"

if [ "$ENABLE_SHARED" = "YES" ]; then
	cp -f lib/.libs/libcurl.$SHARED_LIBRARY_EXT "${TARGET_DIR}"
fi
