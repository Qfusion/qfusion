/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#if !defined ( __APPLE__ )
#include <sys/sendfile.h>
#endif
#include <errno.h>
#include <arpa/inet.h>

#ifdef ALIGN
#undef ALIGN
#endif
#include "../qcommon/qcommon.h"
#include "../qcommon/sys_net.h"

//=============================================================================

/*
* Sys_NET_GetLastError
*/
net_error_t Sys_NET_GetLastError( void ) {
	int _errno = errno == EAGAIN ? EWOULDBLOCK : errno;

	switch( _errno ) {
		case 0:             return NET_ERR_NONE;
		case ECONNREFUSED:  return NET_ERR_CONNRESET;
		case EWOULDBLOCK:   return NET_ERR_WOULDBLOCK;
		case EINPROGRESS:   return NET_ERR_INPROGRESS;
		default:            return NET_ERR_UNKNOWN;
	}
}

//=============================================================================

/*
* Sys_NET_SocketClose
*/
void Sys_NET_SocketClose( socket_handle_t handle ) {
	close( handle );
}

/*
* Sys_NET_SocketIoctl
*/
int Sys_NET_SocketIoctl( socket_handle_t handle, long request, ioctl_param_t* param ) {
	return ioctl( handle, request, param );
}

/*
* Sys_NET_SendFile
*/
int64_t Sys_NET_SendFile( socket_handle_t handle, int fileno, size_t offset, size_t count ) {
	off_t len;
	off_t _offset = offset;
#if defined ( __APPLE__ )
	len = count;
	ssize_t result = sendfile( fileno, handle, _offset, &len, NULL, 0 );
	result = len;
#else
	ssize_t result = sendfile( handle, fileno, &_offset, count );
	len = result;
#endif
	if( result < 0 ) {
		return result;
	}
	return len;
}

//===================================================================

/*
* Sys_NET_Init
*/
void Sys_NET_Init( void ) {
}

/*
* Sys_NET_Shutdown
*/
void Sys_NET_Shutdown( void ) {
}
