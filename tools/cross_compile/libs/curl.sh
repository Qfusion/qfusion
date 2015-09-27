#!/bin/sh

MORE_OPTS=""
[ ! -z "$HOST" ] && MORE_OPTS="${MORE_OPTS} --host=${HOST}"
[ ! -z "$DATA_DIR" ] && MORE_OPTS="${MORE_OPTS} --datadir=${DATA_DIR}"
[ "$ENABLE_SHARED" = "YES" ] && MORE_OPTS="${MORE_OPTS} --enable-shared"
[ "$ENABLE_SHARED" != "YES" ] && MORE_OPTS="${MORE_OPTS} --disable-shared"
[ "$ENABLE_WINSSL" = "YES" ] && MORE_OPTS="${MORE_OPTS} --with-winssl"

cd ${SOURCE_DIR}libsrcs/libcurl && \
MORE_OPTS="${MORE_OPTS} --build=`./config.guess`" \
./configure --with-zlib=`pwd`/../zlib/ \
 --enable-static --enable-threaded-resolver --disable-ldap --disable-ldaps --disable-dict --disable-telet \
 --disable-ftp --disable-tftp --disable-manual --disable-file --without-libidn --enable-ipv6 \
 --disable-gopher --disable-imap --disable-pop3 --disable-smtp --disable-rtsp --disable-telnet \
 --without-libssh2 \
 ${MORE_OPTS} && \
 ${MAKE}

cp -f lib/.libs/libcurl.a "${TARGET_DIR}libcurlstat.a"

if [ "$ENABLE_SHARED" = "YES" ]; then
	cp -f lib/.libs/libcurl.$SHARED_LIBRARY_EXT "${TARGET_DIR}"
fi
