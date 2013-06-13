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

#include "../qcommon/qcommon.h"

#include "../qcommon/sys_net.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <errno.h>
#include <arpa/inet.h>

//=============================================================================

/*
* Sys_NET_GetLastError
*/
net_error_t Sys_NET_GetLastError( void )
{
	switch( errno )
	{
	case 0:				return NET_ERR_NONE;
	case ECONNREFUSED:	return NET_ERR_CONNRESET;
	case EWOULDBLOCK:	return NET_ERR_WOULDBLOCK;
	case EINPROGRESS:	return NET_ERR_INPROGRESS;
	default:			return NET_ERR_UNKNOWN;
	}
}

/*
* Sys_NET_AsyncResolveHostname
*/
void Sys_NET_AsyncResolveHostname( const char *hostname )
{
}

//=============================================================================

/*
* Sys_NET_SocketClose
*/
void Sys_NET_SocketClose( socket_handle_t handle )
{
	close( handle );
}

/*
* Sys_NET_SocketIoctl
*/
int Sys_NET_SocketIoctl( socket_handle_t handle, long request, ioctl_param_t* param )
{
	return ioctl( handle, request, param );
}

//===================================================================

/*
* Sys_NET_Init
*/
void Sys_NET_Init( void )
{
}

/*
* Sys_NET_Shutdown
*/
void Sys_NET_Shutdown( void )
{
}
