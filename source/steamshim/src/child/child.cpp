/*
Copyright (C) 2023 coolelectronics

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <stdlib.h>
#include <thread>

#include "../os.h"
#include "../steamshim.h"
#include "../steamshim_private.h"
#include "ServerBrowser.h"
#include "child_private.h"
#include "steam/isteamfriends.h"
#include "steam/isteamgameserver.h"
#include "steam/isteammatchmaking.h"
#include "steam/isteamnetworking.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/isteamnetworkingutils.h"
#include "steam/isteamremotestorage.h"
#include "steam/isteamugc.h"
#include "steam/isteamuser.h"
#include "steam/isteamutils.h"
#include "steam/steam_api.h"
#include "steam/steam_api_common.h"
#include "steam/steam_gameserver.h"
#include "steam/steamclientpublic.h"

#include "../packet_utils.h"
#include "rpc_async_handles.h"
#include "steam/steamnetworkingtypes.h"
#include "write_utils.h"

static bool GRunServer = false;
static bool GRunClient = false;

static ISteamUserStats *GSteamStats = NULL;
static ISteamUtils *GSteamUtils = NULL;
static ISteamUser *GSteamUser = NULL;
static AppId_t GAppID = 0;
static uint64 GUserID = 0;

static ISteamGameServer *GSteamGameServer = NULL;
static ISteamUGC *GSteamUGC = NULL;
ServerBrowser *GServerBrowser = NULL;
static time_t time_since_last_pump = 0;

static void handleSteamConnectionStatusChanged( steam_cmd_s cmd, SteamNetConnectionStatusChangedCallback_t *pCallback )
{
	struct p2p_net_connection_changed_evt_s evt;
	evt.cmd = cmd;
	evt.hConn = pCallback->m_hConn;
	evt.listenSocket = pCallback->m_info.m_hListenSocket;
	evt.oldState = pCallback->m_eOldState;
	evt.state = pCallback->m_info.m_eState;
	evt.identityRemoteSteamID = pCallback->m_info.m_identityRemote.GetSteamID64();
	write_packet( GPipeWrite, &evt, sizeof( p2p_net_connection_changed_evt_s ) );
}

static void handleRecvMessageRPC( const steam_rpc_shim_common_s *req, SteamNetworkingMessage_t **msgs, int num_messages, uint64_t steamID, uint32_t handle )
{
	if(num_messages == -1) {
		recv_messages_recv_s recv;
		recv.steamID = steamID;
		recv.handle = handle;
		recv.count = 0;
		prepared_rpc_packet( req, &recv );
		write_packet( GPipeWrite, &recv, sizeof( struct recv_messages_recv_s ));
		printf("invalid handle for req: %d\n", req->cmd);
		return;
	}

	size_t total = 0;
	for( int i = 0; i < num_messages; i++ ) {
		total += msgs[i]->GetSize();
	}
	auto recv = (struct recv_messages_recv_s *)malloc( sizeof( struct recv_messages_recv_s ) + total );
	recv->steamID = steamID;
	recv->handle = handle;
	recv->count = num_messages;
	size_t offset = 0;

	for( int i = 0; i < num_messages; i++ ) {
		recv->messageinfo[i].count = msgs[i]->GetSize();
		memcpy( recv->buffer + offset, msgs[i]->GetData(), msgs[i]->GetSize() );
		offset += msgs[i]->GetSize();
	}
	prepared_rpc_packet( req, recv );
	write_packet( GPipeWrite, recv, sizeof( struct recv_messages_recv_s ) + total );
	free( recv );
}

static void processEVT( steam_evt_pkt_s *req, size_t size )
{
	switch( req->common.cmd ) {
		case EVT_HEART_BEAT: {
			time( &time_since_last_pump );
			break;
			default:
				assert( 0 ); // unhandled packet
				break;
		}
	}
}

static void processRPC( steam_rpc_pkt_s *req, size_t size )
{
	switch( req->common.cmd ) {
		case RPC_PUMP: {
			time( &time_since_last_pump );
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_CREATE_WORKSHOP_ITEM: {
			SteamAPICall_t call = GSteamUGC->CreateItem( 0, k_EWorkshopFileTypeGameManagedItem );
			steam_async_push_rpc_shim( call, &req->common );
			break;
		}
		case RPC_AUTHSESSION_TICKET: {
			struct auth_session_ticket_recv_s recv;
			prepared_rpc_packet( &req->common, &recv );
			GSteamUser->GetAuthSessionTicket( recv.ticket, AUTH_TICKET_MAXSIZE, &recv.pcbTicket );
			write_packet( GPipeWrite, &recv, sizeof( struct auth_session_ticket_recv_s ) );
			break;
		}
		case RPC_PERSONA_NAME: {
			const char *name = SteamFriends()->GetPersonaName();
			struct buffer_rpc_s recv;
			prepared_rpc_packet( &req->common, &recv );

			const uint32_t bufferSize = strlen( name ) + 1 + sizeof( struct buffer_rpc_s );
			writePipe( GPipeWrite, &bufferSize, sizeof( uint32_t ) );
			writePipe( GPipeWrite, &recv, sizeof( struct buffer_rpc_s ) );
			writePipe( GPipeWrite, name, strlen( name ) + 1 );
			break;
		}
		case RPC_SRV_P2P_ACCEPT_CONNECTION: {
			struct p2p_accept_connection_recv_s recv;
			EResult res = SteamGameServerNetworkingSockets()->AcceptConnection( req->p2p_accept_connection_req.handle );
			prepared_rpc_packet( &req->common, &recv );
			recv.result = res;
			write_packet( GPipeWrite, &recv, sizeof( struct p2p_accept_connection_recv_s ) );
			break;
		}
		case RPC_REQUEST_LAUNCH_COMMAND: {
			uint8_t buffer[sizeof( struct buffer_rpc_s ) + 1024];
			struct buffer_rpc_s *recv = (struct buffer_rpc_s *)buffer;
			prepared_rpc_packet( &req->common, recv );
			const int numberBytes = SteamApps()->GetLaunchCommandLine( (char *)recv->buf, 1024 );
			assert( numberBytes > 0 );
			write_packet( GPipeWrite, &recv, sizeof( buffer_rpc_s ) + numberBytes );
			break;
		}
		case RPC_BEGIN_AUTH_SESSION: {
			const int result = GSteamGameServer->BeginAuthSession( req->begin_auth_session.authTicket, req->begin_auth_session.cbAuthTicket, (uint64)req->begin_auth_session.steamID );
			struct steam_result_recv_s recv;
			prepared_rpc_packet( &req->common, &recv );
			recv.result = result;
			write_packet( GPipeWrite, &recv, sizeof( steam_result_recv_s ) );
			break;
		}
		case RPC_UPDATE_SERVERINFO: {
			GSteamGameServer->SetAdvertiseServerActive( req->server_info.advertise );
			GSteamGameServer->SetDedicatedServer( req->server_info.dedicated );
			GSteamGameServer->SetMaxPlayerCount( req->server_info.maxPlayerCount );
			GSteamGameServer->SetBotPlayerCount( req->server_info.botPlayerCount );
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_result_recv_s ) );
			break;
		}
		case RPC_ACTIVATE_OVERLAY: {
			SteamFriends()->ActivateGameOverlayToUser( "steamid", (uint64)req->open_overlay.id );
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_UPDATE_SERVERINFO_GAME_DATA: {
			GSteamGameServer->SetGameData( (const char *)req->server_game_data.buf );

			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_UPDATE_SERVERINFO_GAME_DESCRIPTION: {
			GSteamGameServer->SetGameDescription( (const char *)req->server_description.buf );

			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_UPDATE_SERVERINFO_GAME_TAGS: {
			GSteamGameServer->SetGameDescription( (const char *)req->server_tags.buf );

			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_UPDATE_SERVERINFO_MAP_NAME: {
			struct steam_rpc_shim_common_s recv;
			GSteamGameServer->SetGameDescription( (const char *)req->server_map_name.buf );
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_UPDATE_SERVERINFO_MOD_DIR: {
			struct steam_rpc_shim_common_s recv;
			GSteamGameServer->SetGameDescription( (const char *)req->server_mod_dir.buf );
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_UPDATE_SERVERINFO_PRODUCT: {
			struct steam_rpc_shim_common_s recv;
			GSteamGameServer->SetGameDescription( (const char *)req->server_product.buf );
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_UPDATE_SERVERINFO_REGION: {
			struct steam_rpc_shim_common_s recv;
			GSteamGameServer->SetGameDescription( (const char *)req->server_region.buf );
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_UPDATE_SERVERINFO_SERVERNAME: {
			struct steam_rpc_shim_common_s recv;
			GSteamGameServer->SetGameDescription( (const char *)req->server_name.buf );
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_SET_RICH_PRESENCE: {
			char *str[2] = { 0 };
			if( unpack_string_array_null( (char *)req->rich_presence.buf, size - sizeof( struct buffer_rpc_s ), str, 2 ) ) {
				SteamFriends()->SetRichPresence( str[0], str[1] );
			}
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_END_AUTH_SESSION: {
			GSteamGameServer->EndAuthSession( (uint64)req->end_auth_session.id );
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_SRV_P2P_LISTEN: {
			ISteamGameServer *gameServer = SteamGameServer();
			ISteamNetworkingSockets *networkSocket = SteamGameServerNetworkingSockets();
			gameServer->LogOnAnonymous();
			struct p2p_connect_recv_s recv;
			prepared_rpc_packet( &req->common, &recv );
			recv.handle = networkSocket->CreateListenSocketP2P( 0, 0, nullptr );
			write_packet( GPipeWrite, &recv, sizeof( p2p_connect_recv_s ) );
			break;
		}
		case RPC_SRV_P2P_CLOSE_LISTEN: {
			SteamGameServerNetworkingSockets()->CloseListenSocket( req->p2p_disconnect.handle );
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_P2P_CONNECT: {
			SteamNetworkingIdentity id;
			CSteamID steamID = CSteamID( (uint64)req->p2p_connect.steamID );
			id.SetSteamID( steamID );
			struct p2p_connect_recv_s recv;
			prepared_rpc_packet( &req->common, &recv );
			recv.handle = SteamNetworkingSockets()->ConnectP2P( id, 0, 0, nullptr );
			write_packet( GPipeWrite, &recv, sizeof( p2p_connect_recv_s ) );
			break;
		}
		case RPC_SRV_P2P_DISCONNECT: {
			SteamGameServerNetworkingSockets()->CloseConnection( req->p2p_disconnect.handle, k_ESteamNetConnectionEnd_AppException_Generic, "Failed to accept connection", false );
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_P2P_DISCONNECT: {
			SteamNetworkingSockets()->CloseConnection( req->p2p_disconnect.handle, 0, nullptr, false );
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_SRV_P2P_SEND_MESSAGE: {
			assert( req->send_message.count < SDR_MAX_MESSAGE_SIZE );
			SteamGameServerNetworkingSockets()->SendMessageToConnection( req->send_message.handle, req->send_message.buffer, req->send_message.count, req->send_message.messageReliability, nullptr );
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_P2P_SEND_MESSAGE: {
			assert( req->send_message.count < SDR_MAX_MESSAGE_SIZE );
			SteamNetworkingSockets()->SendMessageToConnection( req->send_message.handle, req->send_message.buffer, req->send_message.count, req->send_message.messageReliability, nullptr );
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_SRV_P2P_RECV_MESSAGES: {
			SteamNetworkingMessage_t *msgs[32];
			SteamNetConnectionInfo_t info;
			SteamGameServerNetworkingSockets()->GetConnectionInfo( req->recv_messages.handle, &info );
			const uint64_t steamid = info.m_identityRemote.GetSteamID().ConvertToUint64();
			const int n = SteamGameServerNetworkingSockets()->ReceiveMessagesOnConnection( req->recv_messages.handle, msgs, 1 );
			handleRecvMessageRPC( &req->common, msgs, n, steamid, req->recv_messages.handle );
			break;
		}
		case RPC_P2P_RECV_MESSAGES: {
			SteamNetworkingMessage_t *msgs[32];
			SteamNetConnectionInfo_t info;
			SteamNetworkingSockets()->GetConnectionInfo( req->recv_messages.handle, &info );
			const uint64_t steamid = info.m_identityRemote.GetSteamID().ConvertToUint64();

			const int n = SteamNetworkingSockets()->ReceiveMessagesOnConnection( req->recv_messages.handle, msgs, 1 );
			handleRecvMessageRPC( &req->common, msgs, n, steamid, req->recv_messages.handle );
			break;
		}
		case RPC_GETVOICE: {
			uint32 size;

			SteamUser()->GetAvailableVoice( &size );
			if( size == 0 ) {
				break;
			}

			uint8_t buffer[1024];

			SteamUser()->GetVoice( true, buffer, sizeof buffer, &size );

			auto recv = (struct getvoice_recv_s *)malloc( sizeof( struct getvoice_recv_s ) + size );
			recv->count = size;
			memcpy( recv->buffer, buffer, size );

			prepared_rpc_packet( &req->common, recv );
			write_packet( GPipeWrite, recv, sizeof( struct getvoice_recv_s ) + size );
			break;
		}
		case RPC_DECOMPRESS_VOICE: {
			static uint8 decompressed[VOICE_BUFFER_MAX];

			auto recv = (struct decompress_voice_recv_s *)malloc( sizeof( struct decompress_voice_recv_s ) + sizeof( decompressed ) );

			uint32 size = 0;
			uint32 optimalSampleRate = 11025;
			EVoiceResult e = SteamUser()->DecompressVoice( req->decompress_voice.buffer, req->decompress_voice.count, decompressed, sizeof decompressed, &size, optimalSampleRate );
			if( e != 0 )
				printf( "DecompressVoice failed: %d\n", e );
			memcpy( recv->buffer, decompressed, size );
			recv->count = size;

			prepared_rpc_packet( &req->common, recv );
			write_packet( GPipeWrite, recv, sizeof( struct decompress_voice_recv_s ) + size );
			break;
		}
		case RPC_START_VOICE_RECORDING: {
			SteamUser()->StartVoiceRecording();
			SteamFriends()->SetInGameVoiceSpeaking( SteamUser()->GetSteamID(), true );
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_STOP_VOICE_RECORDING: {
			SteamUser()->StopVoiceRecording();
			SteamFriends()->SetInGameVoiceSpeaking( SteamUser()->GetSteamID(), false );
			struct steam_rpc_shim_common_s recv;
			prepared_rpc_packet( &req->common, &recv );
			write_packet( GPipeWrite, &recv, sizeof( steam_rpc_shim_common_s ) );
			break;
		}
		case RPC_REQUEST_STEAM_ID: {
			struct steam_id_rpc_s recv;
			prepared_rpc_packet( &req->common, &recv );
			recv.id = SteamUser()->GetSteamID().ConvertToUint64();
			write_packet( GPipeWrite, &recv, sizeof( steam_id_rpc_s ) );
			break;
		}
		case RPC_REQUEST_AVATAR: {
			uint8_t buffer[sizeof( struct steam_avatar_recv_s ) + STEAM_MAX_AVATAR_SIZE];
			struct steam_avatar_recv_s *recv = (struct steam_avatar_recv_s *)buffer;
			prepared_rpc_packet( &req->common, recv );
			int handle;

			// will recieve the avatar in a persona request cb
			if( SteamFriends()->RequestUserInformation( (uint64)req->avatar_req.steamID, false ) ) {
				goto fail_avatar_image;
			}
			switch( req->avatar_req.size ) {
				case STEAM_AVATAR_SMALL:
					handle = SteamFriends()->GetSmallFriendAvatar( (uint64)req->avatar_req.steamID );
					break;
				case STEAM_AVATAR_MED:
					handle = SteamFriends()->GetMediumFriendAvatar( (uint64)req->avatar_req.steamID );
					break;
				case STEAM_AVATAR_LARGE:
					handle = SteamFriends()->GetLargeFriendAvatar( (uint64)req->avatar_req.steamID );
					break;
				default:
					recv->width = 0;
					recv->height = 0;
					goto fail_avatar_image;
			}
			SteamUtils()->GetImageSize( handle, &recv->width, &recv->height );
			SteamUtils()->GetImageRGBA( handle, recv->buf, STEAM_MAX_AVATAR_SIZE );
		fail_avatar_image:
			const uint32_t size = ( recv->width * recv->height * 4 ) + sizeof( steam_avatar_recv_s );
			assert( size <= sizeof( buffer ) );
			writePipe( GPipeWrite, &size, sizeof( uint32_t ) );
			writePipe( GPipeWrite, buffer, size );
			break;
		}
		default:
			assert( 0 ); // unhandled packet
			break;
	}
}

static void processSteamServerDispatch()
{
	if( GRunServer ) {
		HSteamPipe steamPipe = SteamGameServer_GetHSteamPipe();
		
		// perform period actions
		SteamAPI_ManualDispatch_RunFrame( steamPipe );

		// poll for callbacks
		CallbackMsg_t callback;
		while( SteamAPI_ManualDispatch_GetNextCallback( steamPipe, &callback ) ) {
			int id = callback.m_iCallback;
			void *data = callback.m_pubParam;
			void *callResultData = 0;
			steam_rpc_shim_common_s rpcCallback;

			if( callback.m_iCallback == SteamAPICallCompleted_t::k_iCallback ) {
				SteamAPICallCompleted_t *callCompleted = (SteamAPICallCompleted_t *)callback.m_pubParam;
				void *callResultData = malloc( callCompleted->m_cubParam );
				bool failed;
				if( SteamAPI_ManualDispatch_GetAPICallResult( steamPipe, callCompleted->m_hAsyncCall, callResultData, callCompleted->m_cubParam, callCompleted->m_iCallback, &failed ) ) {
					if( !steam_async_pop_rpc_shim( callCompleted->m_hAsyncCall, &rpcCallback ) ) {
						free( callResultData );
						SteamAPI_ManualDispatch_FreeLastCallback( steamPipe );
						continue;
					}

					id = callCompleted->m_iCallback;
					data = callResultData;
					// dmLogInfo( "SteamAPICallCompleted_t %llu result %d", callCompleted->m_hAsyncCall, id );
				} else {
					free( callResultData );
					SteamAPI_ManualDispatch_FreeLastCallback( steamPipe );
					// dmLogInfo("SteamAPICallCompleted_t failed to get call result");
					return;
				}
			}

			switch( id ) {
				case SteamServersConnected_t::k_iCallback: {
					printf( "SteamServersConnected_t\n" );
					break;
				}
				case SteamNetConnectionStatusChangedCallback_t::k_iCallback:
					handleSteamConnectionStatusChanged( EVT_SRV_P2P_CONNECTION_CHANGED, (SteamNetConnectionStatusChangedCallback_t *)data );
					break;
				case GSPolicyResponse_t::k_iCallback: {
					GSPolicyResponse_t *pCallback = (GSPolicyResponse_t *)data;
					printf( "VAC enabled: %d\n", pCallback->m_bSecure );
					uint64_t steamID = SteamGameServer()->GetSteamID().ConvertToUint64();
					policy_response_evt_s evt;
					evt.cmd = EVT_SRV_P2P_POLICY_RESPONSE;
					evt.steamID = steamID;
					evt.secure = pCallback->m_bSecure;
					write_packet( GPipeWrite, &evt, sizeof( struct policy_response_evt_s ) );
					break;
				}
			}
			free( callResultData );
			SteamAPI_ManualDispatch_FreeLastCallback( steamPipe );
		}
	}
}

static void processSteamDispatch()
{
	if( GRunClient ) {
		// use a manual callback loop
		// see steam_api.h for details
		HSteamPipe steamPipe = SteamAPI_GetHSteamPipe();

		// perform period actions
		SteamAPI_ManualDispatch_RunFrame( steamPipe );

		// poll for callbacks
		CallbackMsg_t callback;
		while( SteamAPI_ManualDispatch_GetNextCallback( steamPipe, &callback ) ) {
			int id = callback.m_iCallback;
			void *data = callback.m_pubParam;
			void *callResultData = 0;
			steam_rpc_shim_common_s rpcCallback;

			if( callback.m_iCallback == SteamAPICallCompleted_t::k_iCallback ) {
				SteamAPICallCompleted_t *callCompleted = (SteamAPICallCompleted_t *)callback.m_pubParam;
				void *callResultData = malloc( callCompleted->m_cubParam );
				bool failed;
				if( SteamAPI_ManualDispatch_GetAPICallResult( steamPipe, callCompleted->m_hAsyncCall, callResultData, callCompleted->m_cubParam, callCompleted->m_iCallback, &failed ) ) {
					if( !steam_async_pop_rpc_shim( callCompleted->m_hAsyncCall, &rpcCallback ) ) {
						free( callResultData );
						SteamAPI_ManualDispatch_FreeLastCallback( steamPipe );
						continue;
					}

					id = callCompleted->m_iCallback;
					data = callResultData;
					// dmLogInfo( "SteamAPICallCompleted_t %llu result %d", callCompleted->m_hAsyncCall, id );
				} else {
					free( callResultData );
					SteamAPI_ManualDispatch_FreeLastCallback( steamPipe );
					// dmLogInfo("SteamAPICallCompleted_t failed to get call result");
					return;
				}
			}

			switch( id ) {
				case UserStatsReceived_t::k_iCallback: {
					break;
				}
				case SteamNetConnectionStatusChangedCallback_t::k_iCallback: {
					handleSteamConnectionStatusChanged( EVT_P2P_CONNECTION_CHANGED, (SteamNetConnectionStatusChangedCallback_t *)data );
					break;
				}
				case GameRichPresenceJoinRequested_t::k_iCallback: {
					GameRichPresenceJoinRequested_t *pCallback = (GameRichPresenceJoinRequested_t *)data;
					join_request_evt_s evt;
					evt.cmd = EVT_GAME_JOIN;
					evt.steamID = pCallback->m_steamIDFriend.ConvertToUint64();
					memcpy( evt.rgchConnect, pCallback->m_rgchConnect, k_cchMaxRichPresenceValueLength );
					write_packet( GPipeWrite, &evt, sizeof( struct join_request_evt_s ) );
					break;
				}
				case PersonaStateChange_t::k_iCallback: {
					PersonaStateChange_t *pCallback = (PersonaStateChange_t *)data;
					persona_changes_evt_s evt;
					evt.cmd = EVT_PERSONA_CHANGED;
					evt.avatar_changed = pCallback->m_nChangeFlags & k_EPersonaChangeAvatar;
					evt.steamID = pCallback->m_ulSteamID;
					write_packet( GPipeWrite, &evt, sizeof( struct persona_changes_evt_s ) );
					break;
				}
			}
			free( callResultData );
			SteamAPI_ManualDispatch_FreeLastCallback( steamPipe );
		}
	}
}

static void processCommands()
{
	static struct steam_packet_buf packet;
	static size_t cursor = 0;
	while( 1 ) {
		if( time_since_last_pump != 0 ) {
			time_t delta = time( NULL ) - time_since_last_pump;
			if( delta > 5 ) // we haven't gotten a pump in 5 seconds, safe to assume the main process is either dead or unresponsive and we can terminate
				return;
		}

		assert( sizeof( struct steam_packet_buf ) == STEAM_PACKED_RESERVE_SIZE );

		if( !pipeReady( GPipeRead ) ) {
			std::this_thread::sleep_for( std::chrono::microseconds( 1000 ) );
			continue;
		}

		const int bytesRead = readPipe( GPipeRead, packet.buffer + cursor, STEAM_PACKED_RESERVE_SIZE - cursor );
		if( bytesRead > 0 ) {
			cursor += bytesRead;
		} else {
			std::this_thread::sleep_for( std::chrono::microseconds( 1000 ) );
			continue;
		}

		processSteamServerDispatch();
		processSteamDispatch();
	continue_processing:

		if( packet.size > STEAM_PACKED_RESERVE_SIZE - sizeof( uint32_t ) ) {
			// the packet is larger then the reserved size
			return;
		}

		if( cursor < packet.size + sizeof( uint32_t ) ) {
			continue;
		}

		if( packet.common.cmd >= RPC_BEGIN && packet.common.cmd < RPC_END ) {
			processRPC( &packet.rpc_payload, packet.size );
		} else if( packet.common.cmd >= EVT_BEGIN && packet.common.cmd < EVT_END ) {
			processEVT( &packet.evt_payload, packet.size );
		}

		if( cursor > packet.size + sizeof( uint32_t ) ) {
			const size_t packetlen = packet.size + sizeof( uint32_t );
			const size_t remainingLen = cursor - packetlen;
			memmove( packet.buffer, packet.buffer + packet.size + sizeof( uint32_t ), remainingLen );
			cursor = remainingLen;
			goto continue_processing;
		} else {
			cursor = 0;
		}
	}
}

static bool initSteamworks( PipeType fd )
{
	if( GRunClient ) {
		// this can fail for many reasons:
		//  - you forgot a steam_appid.txt in the current working directory.
		//  - you don't have Steam running
		//  - you don't own the game listed in steam_appid.txt
		if( !SteamAPI_Init() )
			return 0;


		GSteamStats = SteamUserStats();
		GSteamUtils = SteamUtils();
		GSteamUser = SteamUser();
		GSteamUGC = SteamUGC();

		GAppID = GSteamUtils ? GSteamUtils->GetAppID() : 0;
		GUserID = GSteamUser ? GSteamUser->GetSteamID().ConvertToUint64() : 0;

		GServerBrowser = new ServerBrowser();
		GServerBrowser->RefreshInternetServers();

		SteamNetworkingUtils()->InitRelayNetworkAccess();
		SteamUser()->StopVoiceRecording();
	}

	if( GRunServer ) {
		// AFAIK you have to do a full server init if you want to use any of the steam utils, even though we never use this port
		if( !SteamGameServer_Init( 0, 0, 0, eServerModeAuthenticationAndSecure, "1.0.0.0" ) )
			printf( "port in use, ignoring\n" );

		GSteamGameServer = SteamGameServer();
		if( !GSteamGameServer )
			return 0;

		// GSLT can be used but seems to do nothing currently?
		// SteamGameServer()->LogOnAnonymous();

		GSteamGameServer->SetAdvertiseServerActive( true );
	}
	
	SteamAPI_ManualDispatch_Init ();

	return 1;
}
static void deinitSteamworks()
{
	if( GRunServer ) {
		SteamGameServer_Shutdown();
	}

	if( GRunClient ) {
		SteamAPI_Shutdown();
	}
}

static int initPipes( void )
{
	char buf[64];
	unsigned long long val;

	if( !getEnvVar( "STEAMSHIM_READHANDLE", buf, sizeof( buf ) ) )
		return 0;
	else if( sscanf( buf, "%llu", &val ) != 1 )
		return 0;
	else
		GPipeRead = (PipeType)val;

	if( !getEnvVar( "STEAMSHIM_WRITEHANDLE", buf, sizeof( buf ) ) )
		return 0;
	else if( sscanf( buf, "%llu", &val ) != 1 )
		return 0;
	else
		GPipeWrite = (PipeType)val;

	return ( ( GPipeRead != NULLPIPE ) && ( GPipeWrite != NULLPIPE ) );
} /* initPipes */

int main( int argc, char **argv )
{
#ifndef _WIN32
	signal( SIGPIPE, SIG_IGN );

	if( argc > 1 && !strcmp( argv[1], "steamdebug" ) ) {
		debug = true;
	}
#endif

	dbgprintf( "Child starting mainline.\n" );

	char buf[64];
	if( !initPipes() )
		fail( "Child init failed.\n" );

	if( getEnvVar( "STEAMSHIM_RUNCLIENT", buf, sizeof( buf ) ) )
		GRunClient = true;
	if( getEnvVar( "STEAMSHIM_RUNSERVER", buf, sizeof( buf ) ) )
		GRunServer = true;

	if( !initSteamworks( GPipeWrite ) ) {
		char failure = 0;
		writePipe( GPipeWrite, &failure, sizeof failure );
		fail( "Failed to initialize Steamworks" );
	}
	char success = 1;
	writePipe( GPipeWrite, &success, sizeof success );

	dbgprintf( "Child in command processing loop.\n" );

	// Now, we block for instructions until the pipe fails (child closed it or
	//  terminated/crashed).
	processCommands();

	dbgprintf( "Child shutting down.\n" );

	// Close our ends of the pipes.
	closePipe( GPipeRead );
	closePipe( GPipeWrite );

	deinitSteamworks();

	return 0;
}

#ifdef _WIN32

// from win_sys
#define MAX_NUM_ARGVS 128
int argc;
char *argv[MAX_NUM_ARGVS];

int CALLBACK WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	if( strstr( GetCommandLineA(), "steamdebug" ) ) {
		debug = true;
		FreeConsole();
		AllocConsole();
		freopen( "CONOUT$", "w", stdout );
	}
	return main( argc, argv );
} // WinMain
#endif
