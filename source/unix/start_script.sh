#!/bin/sh

#### Config

# Location of Warsow binaries?
# Leave empty to use the directory of this script

BINARY_DIR=

# Location of Warsow binaries?
# Leave empty to use the directory of the binaries

DATA_DIR=

# Whether to use ~/.warsow directory? (0 or 1)
# Leave empty to set to 1 if DATA_DIR doesn't have write permissions

USEHOMEDIR=1

##### Code

if [ "X$BINARY_DIR" = "X" ]; then
	BINARY_DIR="`dirname \"${0}\"`"
fi

if [ "X$DATA_DIR" = "X" ]; then
	DATA_DIR="$BINARY_DIR"
fi

if [ "X$USEHOMEDIR" = "X" ]; then
	if [ ! -w "$DATA_DIR" ]; then
		USEHOMEDIR="1"
	else
		USEHOMEDIR="0"
	fi
fi

base_arch=`uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc/ -e s/sparc64/sparc/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/`
os=`uname`

if [ "X${os}" = "XFreeBSD" ]; then
	arch=freebsd_$base_arch
else
	arch=$base_arch
fi

executable="`basename \"${0}\"`.$arch"

if [ ! -e "$BINARY_DIR/$executable" ]; then
	echo "Error: Executable for system '$arch' not found"
	exit 1
fi

exec "$BINARY_DIR/$executable" +set fs_basepath "$DATA_DIR" +set fs_usehomedir "$USEHOMEDIR" "${@}"
