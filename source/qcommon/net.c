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

#include "qcommon.h"

#include "sys_net.h"

#ifdef _WIN32
#include "../win32/winquake.h"
#else
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif

#define	MAX_LOOPBACK	4

#if !defined SHUT_RDWR && defined SD_BOTH
#	define SHUT_RDWR SD_BOTH
#endif

#if ( defined ( __FreeBSD__ ) && ( __FreeBSD_version < 600020 ) || defined ( __APPLE__ ) )
#	define USE_TCP_NOSIGPIPE
#endif

#ifndef MSG_NOSIGNAL
#	define MSG_NOSIGNAL 0
#endif


typedef struct
{
	qbyte data[MAX_MSGLEN];
	int datalen;
} loopmsg_t;

typedef struct
{
	qboolean open;
	loopmsg_t msgs[MAX_LOOPBACK];
	int get, send;
} loopback_t;

static loopback_t loopbacks[2];
static char *errorstring = NULL;
static size_t errorstring_size = 0;
static qboolean	net_initialized = qfalse;

#define MAX_IPS 16
static int numIP;
static qbyte localIP[MAX_IPS][4];

/*
=============================================================================
PRIVATE FUNCTIONS
=============================================================================
*/

/*
* GetLastErrorString
*/
static const char *GetLastErrorString( void )
{
	switch( Sys_NET_GetLastError() )
	{
	case NET_ERR_UNKNOWN:		return "Unknown error";
	case NET_ERR_NONE:			return "No error";

	case NET_ERR_CONNRESET:		return "Connection reset or refused";
	case NET_ERR_INPROGRESS:	return "Operation in progress";
	case NET_ERR_MSGSIZE:		return "Oversized packet";
	case NET_ERR_WOULDBLOCK:	return "Operation should have blocked";
	case NET_ERR_UNSUPPORTED:	return "Unsupported address or protocol";
	default:					return "Unsupported error code";
	}
}

/*
* GetLocalAddress
*/
static void GetLocalAddress( void )
{
	struct hostent *hostInfo;
	char hostname[256];
	char *p;
	int ip;
	int n;

	if( gethostname( hostname, 256 ) == SOCKET_ERROR )
		return;

	hostInfo = gethostbyname( hostname );
	if( !hostInfo )
		return;

	Com_Printf( "Hostname: %s\n", hostInfo->h_name );
	n = 0;
	while( ( p = hostInfo->h_aliases[n++] ) != NULL )
	{
		Com_Printf( "Alias: %s\n", p );
	}

	// FIXME: IPv6?
	if( hostInfo->h_addrtype != AF_INET )
		return;

	numIP = 0;
	while( ( p = hostInfo->h_addr_list[numIP] ) != NULL && numIP < MAX_IPS )
	{
		ip = ntohl( *(int *)p );
		localIP[numIP][0] = p[0];
		localIP[numIP][1] = p[1];
		localIP[numIP][2] = p[2];
		localIP[numIP][3] = p[3];
		Com_Printf( "IP: %i.%i.%i.%i\n", ( ip >> 24 ) & 0xff, ( ip >> 16 ) & 0xff, ( ip >> 8 ) & 0xff, ip & 0xff );
		numIP++;
	}
}

/*
* AddressToSockaddress
*/
static qboolean AddressToSockaddress( const netadr_t *address, struct sockaddr_storage *sadr )
{
	assert( address );
	assert( sadr );

	switch( address->type )
	{
	case NA_IP:
		{
			const netadr_ipv4_t *na4 = &address->address.ipv4;
			struct sockaddr_in *sadr_in = (struct sockaddr_in *)sadr;

			memset( sadr_in, 0, sizeof( *sadr_in ) );
			sadr_in->sin_family = AF_INET;
			sadr_in->sin_port = na4->port;
			sadr_in->sin_addr.s_addr = *(int *)&na4->ip;
			return qtrue;
		}

	case NA_IP6:
		{
			const netadr_ipv6_t *na6 = &address->address.ipv6;
			struct sockaddr_in6 *sadr_in6 = (struct sockaddr_in6 *)sadr;

			memset( sadr_in6, 0, sizeof( *sadr_in6 ) );
			sadr_in6->sin6_family = AF_INET6;
			sadr_in6->sin6_port = na6->port;
			sadr_in6->sin6_scope_id = na6->scope_id;
			memcpy( &sadr_in6->sin6_addr, na6->ip, sizeof( sadr_in6->sin6_addr ) );
			return qtrue;
		}

	default:
		NET_SetErrorString( "Unsupported address type" );
		return qfalse;
	}
}

/*
* SockaddressToAddress
*/
static qboolean SockaddressToAddress( const struct sockaddr *s, netadr_t *address )
{
	assert( s );
	assert( address );

	switch( s->sa_family )
	{
	case AF_INET:
		{
			const struct sockaddr_in *sadr_in = (const struct sockaddr_in *)s;
			netadr_ipv4_t *na4 = &address->address.ipv4;

			address->type = NA_IP;
			*(int*)na4->ip = sadr_in->sin_addr.s_addr;
			na4->port = sadr_in->sin_port;
			return qtrue;
		}

	case AF_INET6:
		{
			const struct sockaddr_in6 *sadr_in6 = (const struct sockaddr_in6 *)s;
			netadr_ipv6_t *na6 = &address->address.ipv6;

			address->type = NA_IP6;
			memcpy( na6->ip, &sadr_in6->sin6_addr, sizeof( na6->ip ) );
			na6->port = sadr_in6->sin6_port;
			na6->scope_id = sadr_in6->sin6_scope_id;
			return qtrue;
		}

	default:
		NET_SetErrorString( "Unknown address family" );
		return qfalse;
	}
}

/*
* BindSocket
*/
static qboolean BindSocket( socket_handle_t handle, const netadr_t *address )
{
	struct sockaddr_storage sockaddress;
	socklen_t addrlen;

	if( !AddressToSockaddress( address, &sockaddress ) )
		return qfalse;

	addrlen = ( sockaddress.ss_family == AF_INET6 ? sizeof( struct sockaddr_in6 ) : sizeof( struct sockaddr_in ) );
	if( bind( handle, (struct sockaddr*)&sockaddress, addrlen ) == SOCKET_ERROR )
	{
		NET_SetErrorStringFromLastError( "bind" );
		return qfalse;
	}

	return qtrue;
}

/*
* OpenSocket
* 
* returns handle or INVALID_SOCKET for error
*/
static socket_handle_t OpenSocket( socket_type_t type, qboolean ipv6 )
{
	socket_handle_t handle;
	int protocol = ( ipv6 ? PF_INET6 : PF_INET );

	switch( type )
	{
	case SOCKET_UDP:
		handle = socket( protocol, SOCK_DGRAM, IPPROTO_UDP );
		if( handle == INVALID_SOCKET )
		{
			NET_SetErrorStringFromLastError( "socket" );
			return INVALID_SOCKET;
		}
		break;

#ifdef TCP_SUPPORT
	case SOCKET_TCP:
		handle = socket( protocol, SOCK_STREAM, IPPROTO_TCP );
		if( handle == INVALID_SOCKET )
		{
			NET_SetErrorStringFromLastError( "socket" );
			return INVALID_SOCKET;
		}
		else
		{
			struct linger ling;

			ling.l_onoff = 1;
			ling.l_linger = 5;		// 0 for abortive disconnect

			if( setsockopt( handle, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof( ling ) ) < 0 )
			{
				NET_SetErrorStringFromLastError( "socket" );
				return INVALID_SOCKET;
			}
		}
		break;
#endif

	default:
		NET_SetErrorString( "Unknown socket type" );
		return INVALID_SOCKET;
	}

	// Win32's API only defines the IPV6_V6ONLY option since Windows Vista, but fortunately
	// the default value is what we want on Win32 anyway (IPV6_V6ONLY = true)
#ifdef IPV6_V6ONLY
	if( ipv6 )
	{
		int ipv6_only = 1;
		setsockopt( handle, IPPROTO_IPV6, IPV6_V6ONLY, (const void*)&ipv6_only, sizeof( ipv6_only ) );
	}
#endif

	return handle;
}

/*
* NET_SocketMakeBroadcastCapable
*/
static qboolean NET_SocketMakeBroadcastCapable( socket_handle_t handle )
{
	int num = 1;

	if( setsockopt( handle, SOL_SOCKET, SO_BROADCAST, (char *)&num, sizeof( num ) ) == SOCKET_ERROR )
	{
		NET_SetErrorStringFromLastError( "setsockopt" );
		return qfalse;
	}

	return qtrue;
}

/*
* NET_SocketMakeNonBlocking
*/
static qboolean NET_SocketMakeNonBlocking( socket_handle_t handle )
{
	ioctl_param_t _true = 1;

	if( Sys_NET_SocketIoctl( handle, FIONBIO, &_true ) == SOCKET_ERROR )
	{
		NET_SetErrorStringFromLastError( "Sys_NET_SocketIoctl" );
		return qfalse;
	}

	return qtrue;
}

/*
* NET_UDP_GetPacket
*/
static int NET_UDP_GetPacket( const socket_t *socket, netadr_t *address, msg_t *message )
{
	struct sockaddr_storage from;
	socklen_t fromlen;
	int ret;

	assert( socket && socket->open && socket->type == SOCKET_UDP );
	assert( address );
	assert( message );
	assert( message->data );
	assert( message->maxsize > 0 );

	fromlen = sizeof( from );
	ret = recvfrom( socket->handle, (char*)message->data, message->maxsize, 0, (struct sockaddr *)&from, &fromlen );
	if( ret == SOCKET_ERROR )
	{
		net_error_t err;

		NET_SetErrorStringFromLastError( "recvfrom" );

		err = Sys_NET_GetLastError();
		if( err == NET_ERR_WOULDBLOCK || err == NET_ERR_CONNRESET )  // would block
			return 0;

		return -1;
	}

	if( !SockaddressToAddress( (struct sockaddr*)&from, address ) )
		return -1;

	if( ret == (int)message->maxsize )
	{
		NET_SetErrorString( "Oversized packet" );
		return -1;
	}

	message->readcount = 0;
	message->cursize = ret;

	return 1;
}

/*
* NET_UDP_SendPacket
*/
static qboolean NET_UDP_SendPacket( const socket_t *socket, const void *data, size_t length, const netadr_t *address )
{
	struct sockaddr_storage addr;
	socklen_t addrlen;

	assert( socket && socket->open && socket->type == SOCKET_UDP );
	assert( data );
	assert( address );
	assert( length > 0 );

	if( !AddressToSockaddress( address, &addr ) )
		return qfalse;

	addrlen = ( addr.ss_family == AF_INET6 ? sizeof( struct sockaddr_in6 ) : sizeof( struct sockaddr_in ) );
	if( sendto( socket->handle, data, length, 0, (struct sockaddr *)&addr, addrlen ) == SOCKET_ERROR )
	{
		NET_SetErrorStringFromLastError( "sendto" );
		return qfalse;
	}

	return qtrue;
}

/*
* NET_IP_OpenSocket
*/
static qboolean NET_IP_OpenSocket( socket_t *sock, const netadr_t *address, socket_type_t socktype, qboolean server )
{
	int newsocket;
	const char *proto, *stype;

	assert( sock && !sock->open );
	assert( address );

	if( address->type == NA_IP )
		proto = "IP";
	else if( address->type == NA_IP6 )
		proto = "IPv6";
	else
	{
		NET_SetErrorString( "Invalid address type" );
		return qfalse;
	}

	if( socktype == SOCKET_UDP )
		stype = "UDP";
#ifdef TCP_SUPPORT
	else if( socktype == SOCKET_TCP )
		stype = "TCP";
#endif
	else
	{
		NET_SetErrorString( "Invalid socket type" );
		return qfalse;
	}

	if( NET_IsAnyAddress( address ) )
	{
		Com_Printf( "Opening %s/%s socket: *:%hu\n", stype, proto, NET_GetAddressPort( address ) );
	}
	else
	{
		Com_Printf( "Opening %s/%s socket: %s\n", stype, proto, NET_AddressToString( address ) );
	}

	if( ( newsocket = OpenSocket( socktype, ( address->type == NA_IP6 ? qtrue : qfalse ) ) ) == INVALID_SOCKET )
		return qfalse;

	// make it non-blocking
	if( !NET_SocketMakeNonBlocking( newsocket ) )
	{
		Sys_NET_SocketClose( newsocket );
		return qfalse;
	}

	if( socktype == SOCKET_UDP )
	{
		// make it broadcast capable
		if( !NET_SocketMakeBroadcastCapable( newsocket ) )
		{
			Sys_NET_SocketClose( newsocket );
			return qfalse;
		}
	}

	// wsw : pb : make it reusable (fast release of port when quit)
	/*if( setsockopt(newsocket, SOL_SOCKET, SO_REUSEADDR, (char *)&i, sizeof(i)) == -1 ) {
	SetErrorStringFromErrno( "setsockopt" );
	return 0;
	}*/

	if( !BindSocket( newsocket, address ) )
	{
		Sys_NET_SocketClose( newsocket );
		return qfalse;
	}

	sock->open = qtrue;
	sock->type = socktype;
	sock->address = *address;
	sock->server = server;
	sock->handle = newsocket;

	return qtrue;
}

/*
* NET_UDP_CloseSocket
*/
static void NET_UDP_CloseSocket( socket_t *socket )
{
	assert( socket && socket->type == SOCKET_UDP );

	if( !socket->open )
		return;

	Sys_NET_SocketClose( socket->handle );
	socket->handle = 0;
	socket->open = qfalse;
}

//=============================================================================

#ifdef TCP_SUPPORT
/*
* NET_TCP_Get
*/
static int NET_TCP_Get( const socket_t *socket, netadr_t *address, void *data, size_t length )
{
	int ret;

	assert( socket && socket->open && socket->type == SOCKET_TCP );
	assert( data );
	assert( length > 0 );

	ret = recv( socket->handle, data, length, 0 );
	if( ret == SOCKET_ERROR )
	{
		NET_SetErrorStringFromLastError( "recv" );
		if( Sys_NET_GetLastError() == NET_ERR_WOULDBLOCK )  // would block
			return 0;
		return -1;
	}

	if( address )
		*address = socket->remoteAddress;

	return ret;
}

/*
* NET_TCP_GetPacket
*/
static int NET_TCP_GetPacket( const socket_t *socket, netadr_t *address, msg_t *message )
{
	int ret;
	qbyte buffer[MAX_PACKETLEN + 4];
	size_t len;

	assert( socket && socket->open && socket->connected && socket->type == SOCKET_TCP );
	assert( address );
	assert( message );

	// peek the message to see if the whole packet is ready
	ret = recv( socket->handle, (char*)buffer, sizeof( buffer ), MSG_PEEK );
	if( ret == SOCKET_ERROR )
	{
		NET_SetErrorStringFromLastError( "recv" );
		if( Sys_NET_GetLastError() == NET_ERR_WOULDBLOCK )  // would block
			return 0;
		return -1;
	}

	if( ret < 4 )  // the length information is not yet received
		return 0;

	memcpy( &len, buffer, 4 );
	len = LittleLong( len );

	if( len > MAX_PACKETLEN || len > message->maxsize )
	{
		NET_SetErrorString( "Oversized packet" );
		return -1;
	}

	if( ret < (int)len + 4 )  // the whole packet is not yet ready
		return 0;

	// ok we have the whole packet ready, get it

	// read the 4 byte header
	ret = NET_TCP_Get( socket, NULL, buffer, 4 );
	if( ret == -1 )
		return -1;
	if( ret != 4 )
	{
		NET_SetErrorString( "Couldn't read the whole packet" );
		return -1;
	}

	ret = NET_TCP_Get( socket, NULL, message->data, len );
	if( ret == -1 )
		return -1;
	if( ret != (int)len )
	{
		NET_SetErrorString( "Couldn't read the whole packet" );
		return -1;
	}

	*address = socket->remoteAddress;

	message->readcount = 0;
	message->cursize = ret;

	return qtrue;
}


/*
* NET_TCP_Send
*/
static qboolean NET_TCP_Send( const socket_t *socket, const void *data, size_t length )
{
#ifdef USE_TCP_NOSIGPIPE
	int opt_val = 1;
#endif
	int ret;

	assert( socket && socket->open && socket->type == SOCKET_TCP );
	assert( data );
	assert( length > 0 );

#ifdef USE_TCP_NOSIGPIPE
	// Disable SIGPIPE
	// Currently ignore the return code from setsockopt
	setsockopt( socket->handle, SOL_SOCKET, SO_NOSIGPIPE, &opt_val, sizeof( opt_val ) );
#endif

	ret = send( socket->handle, data, length, MSG_NOSIGNAL );

#ifdef USE_TCP_NOSIGPIPE
	// Enable SIGPIPE
	opt_val = 0;
	setsockopt( socket->handle, SOL_SOCKET, SO_NOSIGPIPE, &opt_val, sizeof( opt_val ) );
#endif

	if( ret == SOCKET_ERROR )
	{
		NET_SetErrorStringFromLastError( "send" );
		return qfalse;
	}

	if( ret != (int)length )
	{
		NET_SetErrorString( "Couldn't send all data" );
		return qfalse;
	}

	return qtrue;
}

/*
* NET_TCP_Listen
*/
static qboolean NET_TCP_Listen( const socket_t *socket )
{
	assert( socket && socket->open && socket->type == SOCKET_TCP && socket->handle );

	if( listen( socket->handle, 8 ) == -1 )
	{
		NET_SetErrorStringFromLastError( "listen" );
		return qfalse;
	}

	return qtrue;
}

/*
* NET_TCP_Connect
*/
static connection_status_t NET_TCP_Connect( socket_t *socket, const netadr_t *address )
{
	struct sockaddr_storage sockaddress;
	socklen_t addrlen;

	assert( socket && socket->open && socket->type == SOCKET_TCP && socket->handle && !socket->connected );
	assert( address );

	if( !AddressToSockaddress( address, &sockaddress ) )
		return CONNECTION_FAILED;

	addrlen = ( sockaddress.ss_family == AF_INET6 ? sizeof( struct sockaddr_in6 ) : sizeof( struct sockaddr_in ) );
	if( connect( socket->handle, (struct sockaddr*)&sockaddress, addrlen ) == SOCKET_ERROR )
	{
		net_error_t err;

		err = Sys_NET_GetLastError();
		if( err == NET_ERR_INPROGRESS || err == NET_ERR_WOULDBLOCK )
		{
			socket->remoteAddress = *address;
			return CONNECTION_INPROGRESS;
		}
		else
		{
			NET_SetErrorStringFromLastError( "connect" );
			return CONNECTION_FAILED;
		}
	}

	socket->connected = qtrue;
	socket->remoteAddress = *address;

	return CONNECTION_SUCCEEDED;
}

/*
* NET_TCP_CheckConnect
*/
static connection_status_t NET_TCP_CheckConnect( socket_t *socket )
{
	struct timeval timeout = { 0, 0 };
	int result;
	fd_set set;

	assert( socket && socket->open && socket->type == SOCKET_TCP );

	if( socket->connected )
		return CONNECTION_SUCCEEDED;

	FD_ZERO( &set );
	FD_SET( socket->handle, &set );

	if( ( result = select( socket->handle + 1, NULL, &set, NULL, &timeout ) ) == SOCKET_ERROR )
	{
		NET_SetErrorStringFromLastError( "select" );
		return CONNECTION_FAILED;
	}
	else if( result )
	{
		struct sockaddr addr;
		socklen_t addr_size;

		if( !FD_ISSET( socket->handle, &set ) )
		{
			NET_SetErrorString( "Write fd not set" );
			return CONNECTION_FAILED;
		}

		// trick to check if we actually got connection succesfully
		// idea from http://cr.yp.to/docs/connect.html
		addr_size = sizeof( addr );
		if( getpeername( socket->handle, &addr, &addr_size ) != 0 )
		{
			char ch;
			recv( socket->handle, &ch, 1, 0 ); // produces right errno
			NET_SetErrorStringFromLastError( "getpeername" );
			return CONNECTION_FAILED;
		}

		socket->connected = qtrue;

		return CONNECTION_SUCCEEDED;
	}
	else
	{
		return CONNECTION_INPROGRESS;
	}
}

/*
* NET_TCP_Accept
*/
static int NET_TCP_Accept( const socket_t *socket, socket_t *newsocket, netadr_t *address )
{
	struct sockaddr sockaddress;
	socklen_t sockaddress_size;
	int handle;

	assert( socket && socket->open && socket->type == SOCKET_TCP && socket->handle );
	assert( newsocket );
	assert( address );

	sockaddress_size = sizeof( sockaddress );
	handle = accept( socket->handle, &sockaddress, &sockaddress_size );
	if( handle == SOCKET_ERROR )
	{
		NET_SetErrorStringFromLastError( "accept" );
		if( Sys_NET_GetLastError() == NET_ERR_WOULDBLOCK )  // would block
			return 0;
		return -1;
	}

	if( !SockaddressToAddress( &sockaddress, address ) )
		return -1;

	// make the new socket non-blocking
	if( !NET_SocketMakeNonBlocking( handle ) )
	{
		Sys_NET_SocketClose( handle );
		return -1;
	}

	newsocket->open = qtrue;
	newsocket->type = SOCKET_TCP;
	newsocket->server = socket->server;
	newsocket->address = socket->address;
	newsocket->remoteAddress = *address;
	newsocket->handle = handle;

	return 1;
}

/*
* NET_TCP_CloseSocket
*/
static void NET_TCP_CloseSocket( socket_t *socket )
{
	assert( socket && socket->type == SOCKET_TCP );

	if( !socket->open )
		return;

	shutdown( socket->handle, SHUT_RDWR );

	Sys_NET_SocketClose( socket->handle );
	socket->handle = 0;
	socket->open = qfalse;
	socket->connected = qfalse;
}
#endif // TCP_SUPPORT

//===================================================================


/*
* NET_Loopback_GetPacket
*/
static int NET_Loopback_GetPacket( const socket_t *socket, netadr_t *address, msg_t *net_message )
{
	int i;
	loopback_t *loop;

	assert( socket->type == SOCKET_LOOPBACK && socket->open );

	loop = &loopbacks[socket->handle];

	if( loop->send - loop->get > ( MAX_LOOPBACK - 1 ) )  // wsw : jal (from q2pro)
		loop->get = loop->send - MAX_LOOPBACK + 1; // wsw : jal (from q2pro)

	if( loop->get >= loop->send )
		return 0;

	i = loop->get & ( MAX_LOOPBACK-1 );
	loop->get++;

	memcpy( net_message->data, loop->msgs[i].data, loop->msgs[i].datalen );
	net_message->cursize = loop->msgs[i].datalen;
	memset( address, 0, sizeof( *address ) );
	address->type = NA_LOOPBACK;

	return 1;
}

/*
* NET_SendLoopbackPacket
*/
static qboolean NET_Loopback_SendPacket( const socket_t *socket, const void *data, size_t length,
										const netadr_t *address )
{
	int i;
	loopback_t *loop;

	assert( socket->open && socket->type == SOCKET_LOOPBACK );
	assert( data );
	assert( length > 0 );
	assert( address );

	if( address->type != NA_LOOPBACK )
	{
		NET_SetErrorString( "Invalid address" );
		return qfalse;
	}

	loop = &loopbacks[socket->handle^1];

	i = loop->send & ( MAX_LOOPBACK - 1 );
	loop->send++;

	memcpy( loop->msgs[i].data, data, length );
	loop->msgs[i].datalen = length;

	return qtrue;
}

/*
* NET_Loopback_OpenSocket
*/
static qboolean NET_Loopback_OpenSocket( socket_t *socket, const netadr_t *address, qboolean server )
{
	int i;

	assert( address );

	if( address->type != NA_LOOPBACK )
	{
		NET_SetErrorString( "Invalid address" );
		return qfalse;
	}

	for( i = 0; i < 2; i++ )
	{
		if( !loopbacks[i].open )
			break;
	}
	if( i == 2 )
	{
		NET_SetErrorString( "Both loopback sockets already open" );
		return qfalse;
	}

	memset( &loopbacks[i], 0, sizeof( loopbacks[i] ) );
	loopbacks[i].open = qtrue;

	socket->open = qtrue;
	socket->handle = i;

	socket->type = SOCKET_LOOPBACK;
	socket->address = *address;
	socket->server = server;

	return qtrue;
}

/*
* NET_Loopback_CloseSocket
*/
static void NET_Loopback_CloseSocket( socket_t *socket )
{
	assert( socket->type == SOCKET_LOOPBACK );

	if( !socket->open )
		return;

	assert( socket->handle >= 0 && socket->handle < 2 );

	loopbacks[socket->handle].open = qfalse;
	socket->open = qfalse;
	socket->handle = 0;
}

#ifdef TCP_SUPPORT
/*
* NET_TCP_SendPacket
*/
static qboolean NET_TCP_SendPacket( const socket_t *socket, const void *data, size_t length )
{
	int len;

	assert( socket && socket->open && socket->type == SOCKET_TCP );
	assert( data );

	// we send the length of the packet first
	len = LittleLong( length );
	if( !NET_TCP_Send( socket, &len, 4 ) )
		return qfalse;

	if( !NET_TCP_Send( socket, data, length ) )
		return qfalse;

	return qtrue;
}
#endif

/*
=============================================================================
PUBLIC FUNCTIONS
=============================================================================
*/

/*
* NET_GetPacket
* 
* 1	ok
* 0	not ready
* -1	error
*/
int NET_GetPacket( const socket_t *socket, netadr_t *address, msg_t *message )
{
	assert( socket->open );

	if( !socket->open )
		return -1;

	switch( socket->type )
	{
	case SOCKET_LOOPBACK:
		return NET_Loopback_GetPacket( socket, address, message );

	case SOCKET_UDP:
		return NET_UDP_GetPacket( socket, address, message );

#ifdef TCP_SUPPORT
	case SOCKET_TCP:
		return NET_TCP_GetPacket( socket, address, message );
#endif

	default:
		assert( qfalse );
		NET_SetErrorString( "Unknown socket type" );
		return -1;
	}
}

/*
* NET_Get
* 
* 1	ok
* 0	no data ready
* -1	error
*/
int NET_Get( const socket_t *socket, netadr_t *address, void *data, size_t length )
{
	assert( socket->open );

	if( !socket->open )
		return -1;

	switch( socket->type )
	{
	case SOCKET_LOOPBACK:
	case SOCKET_UDP:
		NET_SetErrorString( "Operation not supported by the socket type" );
		return -1;

#ifdef TCP_SUPPORT
	case SOCKET_TCP:
		return NET_TCP_Get( socket, address, data, length );
#endif

	default:
		assert( qfalse );
		NET_SetErrorString( "Unknown socket type" );
		return -1;
	}
}

/*
* NET_SendPacket
*/
qboolean NET_SendPacket( const socket_t *socket, const void *data, size_t length, const netadr_t *address )
{
	assert( socket->open );

	if( !socket->open )
		return qfalse;

	if( address->type == NA_NOTRANSMIT )
		return qtrue;

	switch( socket->type )
	{
	case SOCKET_LOOPBACK:
		return NET_Loopback_SendPacket( socket, data, length, address );

	case SOCKET_UDP:
		return NET_UDP_SendPacket( socket, data, length, address );

#ifdef TCP_SUPPORT
	case SOCKET_TCP:
		return NET_TCP_SendPacket( socket, data, length );
#endif

	default:
		assert( qfalse );
		NET_SetErrorString( "Unknown socket type" );
		return qfalse;
	}
}

/*
* NET_Send
*/
qboolean NET_Send( const socket_t *socket, const void *data, size_t length, const netadr_t *address )
{
	assert( socket->open );

	if( !socket->open )
		return qfalse;

	if( address->type == NA_NOTRANSMIT )
		return qtrue;

	switch( socket->type )
	{
	case SOCKET_LOOPBACK:
	case SOCKET_UDP:
		NET_SetErrorString( "Operation not supported by the socket type" );
		return qfalse;

#ifdef TCP_SUPPORT
	case SOCKET_TCP:
		return NET_TCP_Send( socket, data, length );
#endif

	default:
		assert( qfalse );
		NET_SetErrorString( "Unknown socket type" );
		return qfalse;
	}
}

/*
* NET_AddressToString
*/
char *NET_AddressToString( const netadr_t *a )
{
	static char s[64];

	switch( a->type )
	{
	case NA_NOTRANSMIT:
		Q_strncpyz( s, "no-transmit", sizeof( s ) );
		break;
	case NA_LOOPBACK:
		Q_strncpyz( s, "loopback", sizeof( s ) );
		break;
	case NA_IP:
		{
			const netadr_ipv4_t *adr4 = &a->address.ipv4;
			Q_snprintfz( s, sizeof( s ), "%i.%i.%i.%i:%hu", adr4->ip[0], adr4->ip[1], adr4->ip[2], adr4->ip[3], BigShort( adr4->port ) );
			break;
		}
	case NA_IP6:
		{
			const netadr_ipv6_t *adr6 = &a->address.ipv6;
			Q_snprintfz( s, sizeof( s ), "[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]:%hu",
				adr6->ip[ 0], adr6->ip[ 1], adr6->ip[ 2], adr6->ip[ 3], adr6->ip[ 4], adr6->ip[ 5], adr6->ip[ 6], adr6->ip[ 7],
				adr6->ip[ 8], adr6->ip[ 9], adr6->ip[10], adr6->ip[11], adr6->ip[12], adr6->ip[13], adr6->ip[14], adr6->ip[15],
				BigShort( adr6->port ) );
			break;
		}
	default:
		assert( qfalse );
		Q_strncpyz( s, "unknown", sizeof( s ) );
		break;
	}

	return s;
}

/*
* NET_CompareBaseAddress
* 
* Compares without the port
*/
qboolean NET_CompareBaseAddress( const netadr_t *a, const netadr_t *b )
{
	if( a->type != b->type )
		return qfalse;

	switch( a->type )
	{
	case NA_LOOPBACK:
		return qtrue;

	case NA_IP:
		{
			const netadr_ipv4_t *addr1 = &a->address.ipv4;
			const netadr_ipv4_t *addr2 = &b->address.ipv4;
			if( addr1->ip[0] == addr2->ip[0] && addr1->ip[1] == addr2->ip[1] && addr1->ip[2] == addr2->ip[2] && addr1->ip[3] == addr2->ip[3] )
				return qtrue;
			return qfalse;
		}

	case NA_IP6:
		{
			const netadr_ipv6_t *addr1 = &a->address.ipv6;
			const netadr_ipv6_t *addr2 = &b->address.ipv6;
			return ( ( memcmp( addr1->ip, addr2->ip, sizeof( addr1->ip ) ) == 0 && addr1->scope_id == addr2->scope_id ) ? qtrue : qfalse );
		}

	default:
		assert( qfalse );
		return qfalse;
	}
}

/*
* NET_GetAddressPort
* 
* Return the port of the network address (if relevant), or 0
*/
unsigned short NET_GetAddressPort( const netadr_t *address )
{
	switch( address->type )
	{
	case NA_IP:
		return BigShort( address->address.ipv4.port );

	case NA_IP6:
		return BigShort( address->address.ipv6.port );

	default:
		return 0;
	}
}

/*
* NET_SetAddressPort
* 
* Set the port of the network address
*/
void NET_SetAddressPort( netadr_t *address, unsigned short port )
{
	switch( address->type )
	{
	case NA_IP:
		address->address.ipv4.port = BigShort( port );
		break;

	case NA_IP6:
		address->address.ipv6.port = BigShort( port );
		break;

	default:
		break;
	}
}

/*
* NET_CompareAddress
* 
* Compares with the port
*/
qboolean NET_CompareAddress( const netadr_t *a, const netadr_t *b )
{
	if( a->type != b->type )
		return qfalse;

	switch( a->type )
	{
	case NA_LOOPBACK:
		return qtrue;

	case NA_IP:
		{
			const netadr_ipv4_t *addr1 = &a->address.ipv4;
			const netadr_ipv4_t *addr2 = &b->address.ipv4;

			if( addr1->ip[0] == addr2->ip[0] && addr1->ip[1] == addr2->ip[1] && addr1->ip[2] == addr2->ip[2] && addr1->ip[3] == addr2->ip[3] &&
				BigShort( addr1->port ) == BigShort( addr2->port ) )
			{
				return qtrue;
			}
			return qfalse;
		}

	case NA_IP6:
		{
			const netadr_ipv6_t *addr1 = &a->address.ipv6;
			const netadr_ipv6_t *addr2 = &b->address.ipv6;

			if( memcmp( addr1->ip, addr2->ip, sizeof( addr1->ip ) ) == 0 &&
				addr1->scope_id == addr2->scope_id &&
				BigShort( addr1->port ) == BigShort( addr2->port ) )
			{
				return qtrue;
			}

			return qfalse;
		}

	default:
		assert( qfalse );
		return qfalse;
	}
}

/*
* NET_InitAddress
*/
void NET_InitAddress( netadr_t *address, netadrtype_t type )
{
	memset( address, 0, sizeof( *address ) );
	address->type = type;
}

/*
* NET_BroadcastAddress
*/
void NET_BroadcastAddress( netadr_t *address, int port )
{
	memset( address, 0, sizeof( *address ) );
	address->type = NA_IP;
	*(int*)address->address.ipv4.ip = htonl( INADDR_BROADCAST );
	address->address.ipv4.port = BigShort( port );
}

/*
* ParseAddressString
*/
static qboolean ParseAddressString( const char *str, char* addr_buff, size_t addr_buff_size, char* port_buff, size_t port_buff_size, int *addr_family  )
{
	const char* addr_start;
	const char* addr_end = NULL;
	const char* port_name = "0";
	int family = AF_UNSPEC;
	size_t addr_length;

	// If it's a bracketed IPv6 address
	if( str[0] == '[' )
	{
		const char* end_bracket = strchr( str, ']' );

		if( end_bracket == NULL )
			return qfalse;

		// If there's something else than a colon after the closing bracket
		if( end_bracket[1] != ':' && end_bracket[1] != '\0' )
			return qfalse;

		// If there's a port number after the address
		if( end_bracket[1] == ':' )
			port_name = end_bracket + 2;

		family = AF_INET6;
		addr_start = str + 1;
		addr_end = end_bracket;
	}
	else
	{
		const char *first_colon;

		addr_start = str;

		// If it's a numeric non-bracket IPv6 address (-> no port),
		// or it's a numeric IPv4 address, or a name, with a port
		first_colon = strchr( str, ':' );
		if( first_colon != NULL )
		{
			const char* last_colon = strrchr( first_colon + 1, ':' );

			// If it's an numeric IPv4 address, or a name, with a port
			if( last_colon == NULL )
			{
				addr_end = first_colon;
				port_name = first_colon + 1;
			}
			else
				family = AF_INET6;
		}
	}

	if( addr_end != NULL )
		addr_length = addr_end - addr_start;
	else
		addr_length = strlen( addr_start );

	// Check the address length
	if( addr_length >= addr_buff_size )
		return qfalse;

	memcpy( addr_buff, addr_start, addr_length );
	addr_buff[ addr_length ] = '\0';

	Q_strncpyz( port_buff, port_name, port_buff_size );

	*addr_family = family;

	return qtrue;
}

/*
* StringToSockaddress
*/
static qboolean StringToSockaddress( const char *s, struct sockaddr_storage *sadr )
{
	char addr_copy [128];
	char port_copy [8];
	const char *str;
	int addr_family;

	assert( s );
	assert( sadr );

	if( strlen( s ) >= sizeof( addr_copy ) / sizeof( char ) )
	{
		NET_SetErrorString( "String too long" );
		return qfalse;
	}

	str = ( s[0] == '\0' ? "0.0.0.0" : s );
	if( ParseAddressString( str, addr_copy, sizeof( addr_copy ), port_copy, sizeof( port_copy ), &addr_family ) )
	{
		struct addrinfo hints;
		struct addrinfo* addrinf = NULL;
		int err;

		memset( &hints, 0, sizeof ( hints ) );
		hints.ai_family = addr_family;
		hints.ai_socktype = SOCK_DGRAM;

		err = getaddrinfo( addr_copy, port_copy, &hints, &addrinf );
		if ( err == 0 && addrinf != NULL )
		{
			memcpy( sadr, addrinf->ai_addr, addrinf->ai_addrlen );
			freeaddrinfo( addrinf );
			return qtrue;
		}
		else
		{
			NET_SetErrorString( "Host not found" );
		}

		if ( addrinf != NULL )
			freeaddrinfo ( addrinf );
	}
	else
	{
		NET_SetErrorString( "Invalid address string" );
	}

	return qfalse;
}

/*
* NET_StringToAddress
*/
qboolean NET_StringToAddress( const char *s, netadr_t *address )
{
	struct sockaddr_storage sadr;

	assert( s );
	assert( address );

	memset( address, 0, sizeof( *address ) );

	if( !StringToSockaddress( s, &sadr ) )
	{
		address->type = NA_NOTRANSMIT;
		return qfalse;
	}

	SockaddressToAddress( (struct sockaddr*)&sadr, address );

	return qtrue;
}

/*
* NET_IsLocalAddress
*/
qboolean NET_IsLocalAddress( const netadr_t *address )
{
	switch( address->type )
	{
	case NA_LOOPBACK:
		return qtrue;

	case NA_IP:
		if( address->address.ipv4.ip[0] == 127 && address->address.ipv4.ip[1] == 0 )
			return qtrue;
		// TODO: Check for own external IP address?
		return qfalse;

	case NA_IP6:
		return ( memcmp( address->address.ipv6.ip, &in6addr_loopback.s6_addr, sizeof( address->address.ipv6.ip ) ) == 0 ) ? qtrue : qfalse;

	default:
		return qfalse;
	}
}

/*
* NET_IsAnyAddress
*/
qboolean NET_IsAnyAddress( const netadr_t *address )
{
	switch( address->type )
	{
	case NA_IP:
		return ( *(unsigned int*)address->address.ipv4.ip == htonl( INADDR_ANY ) ? qtrue : qfalse );

	case NA_IP6:
		return ( memcmp( address->address.ipv6.ip, &in6addr_any.s6_addr, sizeof( address->address.ipv6.ip ) ) == 0 ) ? qtrue : qfalse;

	default:
		return qfalse;
	}
}

/*
* NET_IsLANAddress
*
* FIXME: This function apparently doesn't support CIDR
*/
qboolean NET_IsLANAddress( const netadr_t *address )
{
	if( NET_IsLocalAddress( address ) )
		return qtrue;

	switch( address->type )
	{
	case NA_IP:
		{
			const netadr_ipv4_t *addr4 = &address->address.ipv4;

			// RFC1918:
			// 10.0.0.0        -   10.255.255.255  (10/8 prefix)
			// 172.16.0.0      -   172.31.255.255  (172.16/12 prefix)
			// 192.168.0.0     -   192.168.255.255 (192.168/16 prefix)
			if( addr4->ip[0] == 10 )
				return qtrue;
			if( addr4->ip[0] == 172 && ( addr4->ip[1]&0xf0 ) == 16 )
				return qtrue;
			if( addr4->ip[0] == 192 && addr4->ip[1] == 168 )
				return qtrue;
		}

	case NA_IP6:
		{
			const netadr_ipv6_t *addr6 = &address->address.ipv6;

			// Local addresses are either the loopback adress (tested earlier), or fe80::/10
			if ( addr6->ip[0] == 0xFE && ( addr6->ip[1] & 0xC0 ) == 0x80 ) {
				return qtrue;
			}

			// private address space
			if ( ( addr6->ip[0] & 0xFE ) == 0xFC ) {
				return qtrue;
			}
		}

	default:
		return qfalse;
	}

	return qfalse;
}

/*
* NET_AsyncResolveHostname
*/
void NET_AsyncResolveHostname( const char *hostname )
{
	Sys_NET_AsyncResolveHostname( hostname );
}

/*
* NET_ShowIP
*/
void NET_ShowIP( void )
{
	int i;

	for( i = 0; i < numIP; i++ )
		Com_Printf( "IP: %i.%i.%i.%i\n", localIP[i][0], localIP[i][1], localIP[i][2], localIP[i][3] );
}

/*
* NET_ErrorString
*/
const char *NET_ErrorString( void )
{
	return errorstring;
}

/*
* NET_SetErrorString
*/
void NET_SetErrorString( const char *format, ... )
{
	va_list	argptr;
	char msg[MAX_PRINTMSG];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	if( errorstring_size < strlen( msg ) + 1 )
	{
		if( errorstring )
			Mem_ZoneFree( errorstring );
		errorstring_size = strlen( msg ) + 1 + 64;
		errorstring = ( char* )Mem_ZoneMalloc( errorstring_size );
	}

	Q_strncpyz( errorstring, msg, errorstring_size );
}

/*
* NET_SetErrorStringFromLastError
*/
void NET_SetErrorStringFromLastError( const char *function )
{
	const char* errorstring = GetLastErrorString();
	if( function )
	{
		NET_SetErrorString( "%s: %s", function, errorstring );
	}
	else
	{
		NET_SetErrorString( "%s", errorstring );
	}
}

/*
* NET_SocketTypeToString
*/
const char *NET_SocketTypeToString( socket_type_t type )
{
	switch( type )
	{
	case SOCKET_LOOPBACK:
		return "loopback";

	case SOCKET_UDP:
		return "UDP";

#ifdef TCP_SUPPORT
	case SOCKET_TCP:
		return "TCP";
#endif

	default:
		return "unknown";
	}
}

/*
* NET_SocketToString
*/
const char *NET_SocketToString( const socket_t *socket )
{
	return va( "%s %s", NET_SocketTypeToString( socket->type ), ( socket->server ? "server" : "client" ) );
}

#ifdef TCP_SUPPORT
/*
* NET_Listen
*/
qboolean NET_Listen( const socket_t *socket )
{
	assert( socket->open );

	switch( socket->type )
	{
	case SOCKET_TCP:
		return NET_TCP_Listen( socket );

	case SOCKET_LOOPBACK:
	case SOCKET_UDP:
	default:
		assert( qfalse );
		NET_SetErrorString( "Unsupported socket type" );
		return qfalse;
	}
}

/*
* NET_Connect
*/
connection_status_t NET_Connect( socket_t *socket, const netadr_t *address )
{
	assert( socket->open && !socket->connected );
	assert( address );

	switch( socket->type )
	{
	case SOCKET_TCP:
		return NET_TCP_Connect( socket, address );

	case SOCKET_LOOPBACK:
	case SOCKET_UDP:
	default:
		assert( qfalse );
		NET_SetErrorString( "Unsupported socket type" );
		return CONNECTION_FAILED;
	}
}

/*
* NET_CheckConnect
*/
connection_status_t NET_CheckConnect( socket_t *socket )
{
	assert( socket->open );

	if( socket->connected )
		return CONNECTION_SUCCEEDED;

	switch( socket->type )
	{
	case SOCKET_TCP:
		return NET_TCP_CheckConnect( socket );

	case SOCKET_LOOPBACK:
	case SOCKET_UDP:
	default:
		assert( qfalse );
		NET_SetErrorString( "Unsupported socket type" );
		return CONNECTION_FAILED;
	}
}

/*
* NET_Accept
*/
int NET_Accept( const socket_t *socket, socket_t *newsocket, netadr_t *address )
{
	assert( socket && socket->open );
	assert( newsocket );
	assert( address );

	switch( socket->type )
	{
	case SOCKET_TCP:
		return NET_TCP_Accept( socket, newsocket, address );

	case SOCKET_LOOPBACK:
	case SOCKET_UDP:
	default:
		assert( qfalse );
		NET_SetErrorString( "Unsupported socket type" );
		return qfalse;
	}
}
#endif

/*
* NET_OpenSocket
*/
qboolean NET_OpenSocket( socket_t *socket, socket_type_t type, const netadr_t *address, qboolean server )
{
	assert( !socket->open );
	assert( address );

	switch( type )
	{
	case SOCKET_LOOPBACK:
		return NET_Loopback_OpenSocket( socket, address, server );

#ifdef TCP_SUPPORT
	case SOCKET_TCP:
#endif
	case SOCKET_UDP:
		return NET_IP_OpenSocket( socket, address, type, server );

	default:
		assert( qfalse );
		NET_SetErrorString( "Unknown socket type" );
		return qfalse;
	}
}

/*
* NET_CloseSocket
*/
void NET_CloseSocket( socket_t *socket )
{
	if( !socket->open )
		return;

	switch( socket->type )
	{
	case SOCKET_LOOPBACK:
		NET_Loopback_CloseSocket( socket );
		break;

	case SOCKET_UDP:
		NET_UDP_CloseSocket( socket );
		break;

#ifdef TCP_SUPPORT
	case SOCKET_TCP:
		NET_TCP_CloseSocket( socket );
		break;
#endif

	default:
		assert( qfalse );
		NET_SetErrorString( "Unknown socket type" );
		break;
	}
}

/*
* NET_Sleep
*/
void NET_Sleep( int msec, socket_t *sockets[] )
{
	struct timeval timeout;
	fd_set fdset;
	int i;

	if( !sockets || !sockets[0] )
		return;

	FD_ZERO( &fdset );

	for( i = 0; sockets[i]; i++ )
	{
		assert( sockets[i]->open );

		switch( sockets[i]->type )
		{
		case SOCKET_UDP:
#ifdef TCP_SUPPORT
		case SOCKET_TCP:
#endif
			assert( sockets[i]->handle > 0 );
			FD_SET( (unsigned)sockets[i]->handle, &fdset ); // network socket
			break;

		default:
			Com_Printf( "Warning: Invalid socket type on Sys_NET_Sleep\n" );
			return;
		}
	}

	timeout.tv_sec = msec / 1000;
	timeout.tv_usec = ( msec % 1000 ) * 1000;
	select( FD_SETSIZE, &fdset, NULL, NULL, &timeout );
}

/*
* NET_Monitor
* Monitors the given sockets with the given timeout in milliseconds
* It ignores closed and loopback sockets.
* Calls the callback function read_cb(socket_t *) with the socket as parameter the socket when incoming data was detected on it
* Calls the callback function exception_cb(socket_t *) with the socket as parameter when a socket exception was detected on that socket
* For both callbacks, NULL can be passed. When NULL is passed for the exception_cb, no exception detection is performed
* Incoming data is always detected, even if the 'read_cb' callback was NULL.
*/
int NET_Monitor( int msec, socket_t *sockets[], void (*read_cb)(socket_t *socket), void (*exception_cb)(socket_t *socket) )
{
	struct timeval timeout;
	fd_set fdsetr, fdsete;
	fd_set *p_fdsete = NULL;
	int i, ret;

	if( !sockets || !sockets[0] )
		return 0;

	FD_ZERO( &fdsetr );
	if (exception_cb) {
		FD_ZERO( &fdsete );
		p_fdsete = &fdsete;
	}

	for( i = 0; sockets[i]; i++ )
	{
		if (!sockets[i]->open)
			continue;
		switch( sockets[i]->type )
		{
		case SOCKET_UDP:
#ifdef TCP_SUPPORT
		case SOCKET_TCP:
#endif
			assert( sockets[i]->handle > 0 );
			FD_SET(sockets[i]->handle, &fdsetr ); // network socket
			if (p_fdsete)
				FD_SET(sockets[i]->handle, p_fdsete );
			break;
		case SOCKET_LOOPBACK:
		default:
			continue;
		}
	}

	timeout.tv_sec = msec / 1000;
	timeout.tv_usec = ( msec % 1000 ) * 1000;
	ret = select( FD_SETSIZE, &fdsetr, NULL, p_fdsete, &timeout );
	if ( ( ret > 0) && ( (read_cb) || (exception_cb)) ) {
		// Launch callbacks
		for( i = 0; sockets[i]; i++ ) {
			if (!sockets[i]->open)
				continue;

			switch( sockets[i]->type ) {
			case SOCKET_UDP:
#ifdef TCP_SUPPORT
			case SOCKET_TCP:
#endif
				if ( (exception_cb) && (p_fdsete) && (FD_ISSET(sockets[i]->handle, p_fdsete )) ) {
					exception_cb(sockets[i]);
				}
				if ( (read_cb) && (FD_ISSET(sockets[i]->handle, &fdsetr )) ) {
					read_cb(sockets[i]);
				}
				break;
			case SOCKET_LOOPBACK:
			default:
				continue;
			}
		}
	}
	return ret;
}

/*
* NET_Init
*/
void NET_Init( void )
{
	assert( !net_initialized );

	Sys_NET_Init();

	GetLocalAddress();

	net_initialized = qtrue;
}

/*
* NET_Shutdown
*/
void NET_Shutdown( void )
{
	if( !net_initialized )
		return;

	if( errorstring )
	{
		Mem_ZoneFree( errorstring );
		errorstring = NULL;
		errorstring_size = 0;
	}

	Sys_NET_Shutdown();

	net_initialized = qfalse;
}
