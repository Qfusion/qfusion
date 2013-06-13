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
// net_wins.c

#include "../qcommon/qcommon.h"

#include "../qcommon/sys_net.h"

#include "winquake.h"

//=============================================================================

/*
* Sys_NET_GetLastError
*/
net_error_t Sys_NET_GetLastError( void )
{
	switch( WSAGetLastError() )
	{
	case 0:					return NET_ERR_NONE;
	case WSAEMSGSIZE:		return NET_ERR_MSGSIZE;
	case WSAECONNRESET:		return NET_ERR_CONNRESET;
	case WSAEWOULDBLOCK:	return NET_ERR_WOULDBLOCK;
	case WSAEAFNOSUPPORT:	return NET_ERR_UNSUPPORTED;
	default:				return NET_ERR_UNKNOWN;
	}
}

/*
* Sys_NET_AsyncResolveHostname
*/
void Sys_NET_AsyncResolveHostname( const char *hostname )
{
#ifndef DEDICATED_ONLY
#define WM_ASYNC_LOOKUP_DONE WM_USER+1
	static char hostentbuf[MAXGETHOSTSTRUCT];

	WSAAsyncGetHostByName( cl_hwnd, WM_ASYNC_LOOKUP_DONE, hostname, hostentbuf, sizeof( hostentbuf ) );
#undef WM_ASYNC_LOOKUP_DONE
#endif
}

//=============================================================================

/*
* Sys_NET_SocketClose
*/
void Sys_NET_SocketClose( socket_handle_t handle )
{
	closesocket( handle );
}

/*
* Sys_NET_SocketIoctl
*/
int Sys_NET_SocketIoctl( socket_handle_t handle, long request, ioctl_param_t* param )
{
	return ioctlsocket( handle, request, param );
}

//===================================================================

/*
* Sys_NET_Init
*/
void Sys_NET_Init( void )
{
	WSADATA	winsockdata;

	if( WSAStartup( MAKEWORD( 2, 2 ), &winsockdata ) )
		Com_Error( ERR_FATAL, "Winsock initialization failed" );

	Com_Printf( "Winsock initialized\n" );
}

/*
* Sys_NET_Shutdown
*/
void Sys_NET_Shutdown( void )
{
	WSACleanup();
}
