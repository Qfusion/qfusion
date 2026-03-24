/*
Copyright (C) 2023 velziee

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

#ifndef _INCL_STEAMSHIM_TYPES_H_
#define _INCL_STEAMSHIM_TYPES_H_

#include "../../gameshared/q_arch.h"

#define STEAM_AVATAR_SIZE ( 128 * 128 * 4 )
#define PIPEMESSAGE_MAX ( STEAM_AVATAR_SIZE + 64 )
#define AUTH_TICKET_MAXSIZE 1024
#define SDR_MAX_MESSAGE_SIZE 32768
typedef struct {
	char pTicket[AUTH_TICKET_MAXSIZE];
	long long pcbTicket;
} SteamAuthTicket_t;

#define VOICE_SAMPLE_RATE 11025
#define VOICE_BUFFER_MAX 22000
#define STEAM_MAX_AVATAR_SIZE ( 128 * 128 * 4 )
#define STEAM_AUTH_TICKET_MAXSIZE 1024

enum steam_avatar_size_e { STEAM_AVATAR_LARGE, STEAM_AVATAR_MED, STEAM_AVATAR_SMALL };

enum steam_result_e {
	STEAMSHIM_EResultNone = 0,					 // no result
	STEAMSHIM_EResultOK = 1,					 // success
	STEAMSHIM_EResultFail = 2,					 // generic failure
	STEAMSHIM_EResultNoConnection = 3,			 // no/failed network connection
										 //	k_EResultNoConnectionRetry = 4,				// OBSOLETE - removed
	STEAMSHIM_EResultInvalidPassword = 5,		 // password/ticket is invalid
	STEAMSHIM_EResultLoggedInElsewhere = 6,		 // same user logged in elsewhere
	STEAMSHIM_EResultInvalidProtocolVer = 7,	 // protocol version is incorrect
	STEAMSHIM_EResultInvalidParam = 8,			 // a parameter is incorrect
	STEAMSHIM_EResultFileNotFound = 9,			 // file was not found
	STEAMSHIM_EResultBusy = 10,					 // called method busy - action not taken
	STEAMSHIM_EResultInvalidState = 11,			 // called object was in an invalid state
	STEAMSHIM_EResultInvalidName = 12,			 // name is invalid
	STEAMSHIM_EResultInvalidEmail = 13,			 // email is invalid
	STEAMSHIM_EResultDuplicateName = 14,		 // name is not unique
	STEAMSHIM_EResultAccessDenied = 15,			 // access is denied
	STEAMSHIM_EResultTimeout = 16,				 // operation timed out
	STEAMSHIM_EResultBanned = 17,				 // VAC2 banned
	STEAMSHIM_EResultAccountNotFound = 18,		 // account not found
	STEAMSHIM_EResultInvalidSteamID = 19,		 // steamID is invalid
	STEAMSHIM_EResultServiceUnavailable = 20,	 // The requested service is currently unavailable
	STEAMSHIM_EResultNotLoggedOn = 21,			 // The user is not logged on
	STEAMSHIM_EResultPending = 22,				 // Request is pending (may be in process, or waiting on third party)
	STEAMSHIM_EResultEncryptionFailure = 23,	 // Encryption or Decryption failed
	STEAMSHIM_EResultInsufficientPrivilege = 24, // Insufficient privilege
	STEAMSHIM_EResultLimitExceeded = 25,		 // Too much of a good thing
	STEAMSHIM_EResultRevoked = 26,				 // Access has been revoked (used for revoked guest passes)
	STEAMSHIM_EResultExpired = 27,				 // License/Guest pass the user is trying to access is expired
	STEAMSHIM_EResultAlreadyRedeemed = 28,		 // Guest pass has already been redeemed by account, cannot be acked again
	STEAMSHIM_EResultDuplicateRequest = 29,		 // The request is a duplicate and the action has already occurred in the past, ignored this time
	STEAMSHIM_EResultAlreadyOwned = 30,			 // All the games in this guest pass redemption request are already owned by the user
	STEAMSHIM_EResultIPNotFound = 31,			 // IP address not found
	STEAMSHIM_EResultPersistFailed = 32,		 // failed to write change to the data store
	STEAMSHIM_EResultLockingFailed = 33,		 // failed to acquire access lock for this operation
	STEAMSHIM_EResultLogonSessionReplaced = 34,
	STEAMSHIM_EResultConnectFailed = 35,
	STEAMSHIM_EResultHandshakeFailed = 36,
	STEAMSHIM_EResultIOFailure = 37,
	STEAMSHIM_EResultRemoteDisconnect = 38,
	STEAMSHIM_EResultShoppingCartNotFound = 39, // failed to find the shopping cart requested
	STEAMSHIM_EResultBlocked = 40,				// a user didn't allow it
	STEAMSHIM_EResultIgnored = 41,				// target is ignoring sender
	STEAMSHIM_EResultNoMatch = 42,				// nothing matching the request found
	STEAMSHIM_EResultAccountDisabled = 43,
	STEAMSHIM_EResultServiceReadOnly = 44,				 // this service is not accepting content changes right now
	STEAMSHIM_EResultAccountNotFeatured = 45,			 // account doesn't have value, so this feature isn't available
	STEAMSHIM_EResultAdministratorOK = 46,				 // allowed to take this action, but only because requester is admin
	STEAMSHIM_EResultContentVersion = 47,				 // A Version mismatch in content transmitted within the Steam protocol.
	STEAMSHIM_EResultTryAnotherCM = 48,					 // The current CM can't service the user making a request, user should try another.
	STEAMSHIM_EResultPasswordRequiredToKickSession = 49, // You are already logged in elsewhere, this cached credential login has failed.
	STEAMSHIM_EResultAlreadyLoggedInElsewhere = 50,		 // You are already logged in elsewhere, you must wait
	STEAMSHIM_EResultSuspended = 51,					 // Long running operation (content download) suspended/paused
	STEAMSHIM_EResultCancelled = 52,					 // Operation canceled (typically by user: content download)
	STEAMSHIM_EResultDataCorruption = 53,				 // Operation canceled because data is ill formed or unrecoverable
	STEAMSHIM_EResultDiskFull = 54,						 // Operation canceled - not enough disk space.
	STEAMSHIM_EResultRemoteCallFailed = 55,				 // an remote call or IPC call failed
	STEAMSHIM_EResultPasswordUnset = 56,				 // Password could not be verified as it's unset server side
	STEAMSHIM_EResultExternalAccountUnlinked = 57,		 // External account (PSN, Facebook...) is not linked to a Steam account
	STEAMSHIM_EResultPSNTicketInvalid = 58,				 // PSN ticket was invalid
	STEAMSHIM_EResultExternalAccountAlreadyLinked = 59,	 // External account (PSN, Facebook...) is already linked to some other account, must explicitly request to replace/delete the link first
	STEAMSHIM_EResultRemoteFileConflict = 60,			 // The sync cannot resume due to a conflict between the local and remote files
	STEAMSHIM_EResultIllegalPassword = 61,				 // The requested new password is not legal
	STEAMSHIM_EResultSameAsPreviousValue = 62,			 // new value is the same as the old one ( secret question and answer )
	STEAMSHIM_EResultAccountLogonDenied = 63,			 // account login denied due to 2nd factor authentication failure
	STEAMSHIM_EResultCannotUseOldPassword = 64,			 // The requested new password is not legal
	STEAMSHIM_EResultInvalidLoginAuthCode = 65,			 // account login denied due to auth code invalid
	STEAMSHIM_EResultAccountLogonDeniedNoMail = 66,		 // account login denied due to 2nd factor auth failure - and no mail has been sent
	STEAMSHIM_EResultHardwareNotCapableOfIPT = 67,		 //
	STEAMSHIM_EResultIPTInitError = 68,					 //
	STEAMSHIM_EResultParentalControlRestricted = 69,	 // operation failed due to parental control restrictions for current user
	STEAMSHIM_EResultFacebookQueryError = 70,			 // Facebook query returned an error
	STEAMSHIM_EResultExpiredLoginAuthCode = 71,			 // account login denied due to auth code expired
	STEAMSHIM_EResultIPLoginRestrictionFailed = 72,
	STEAMSHIM_EResultAccountLockedDown = 73,
	STEAMSHIM_EResultAccountLogonDeniedVerifiedEmailRequired = 74,
	STEAMSHIM_EResultNoMatchingURL = 75,
	STEAMSHIM_EResultBadResponse = 76,						   // parse failure, missing field, etc.
	STEAMSHIM_EResultRequirePasswordReEntry = 77,			   // The user cannot complete the action until they re-enter their password
	STEAMSHIM_EResultValueOutOfRange = 78,					   // the value entered is outside the acceptable range
	STEAMSHIM_EResultUnexpectedError = 79,					   // something happened that we didn't expect to ever happen
	STEAMSHIM_EResultDisabled = 80,							   // The requested service has been configured to be unavailable
	STEAMSHIM_EResultInvalidCEGSubmission = 81,				   // The set of files submitted to the CEG server are not valid !
	STEAMSHIM_EResultRestrictedDevice = 82,					   // The device being used is not allowed to perform this action
	STEAMSHIM_EResultRegionLocked = 83,						   // The action could not be complete because it is region restricted
	STEAMSHIM_EResultRateLimitExceeded = 84,				   // Temporary rate limit exceeded, try again later, different from k_EResultLimitExceeded which may be permanent
	STEAMSHIM_EResultAccountLoginDeniedNeedTwoFactor = 85,	   // Need two-factor code to login
	STEAMSHIM_EResultItemDeleted = 86,						   // The thing we're trying to access has been deleted
	STEAMSHIM_EResultAccountLoginDeniedThrottle = 87,		   // login attempt failed, try to throttle response to possible attacker
	STEAMSHIM_EResultTwoFactorCodeMismatch = 88,			   // two factor code mismatch
	STEAMSHIM_EResultTwoFactorActivationCodeMismatch = 89,	   // activation code for two-factor didn't match
	STEAMSHIM_EResultAccountAssociatedToMultiplePartners = 90, // account has been associated with multiple partners
	STEAMSHIM_EResultNotModified = 91,						   // data not modified
	STEAMSHIM_EResultNoMobileDevice = 92,					   // the account does not have a mobile device associated with it
	STEAMSHIM_EResultTimeNotSynced = 93,					   // the time presented is out of range or tolerance
	STEAMSHIM_EResultSmsCodeFailed = 94,					   // SMS code failure (no match, none pending, etc.)
	STEAMSHIM_EResultAccountLimitExceeded = 95,				   // Too many accounts access this resource
	STEAMSHIM_EResultAccountActivityLimitExceeded = 96,		   // Too many changes to this account
	STEAMSHIM_EResultPhoneActivityLimitExceeded = 97,		   // Too many changes to this phone
	STEAMSHIM_EResultRefundToWallet = 98,					   // Cannot refund to payment method, must use wallet
	STEAMSHIM_EResultEmailSendFailure = 99,					   // Cannot send an email
	STEAMSHIM_EResultNotSettled = 100,						   // Can't perform operation till payment has settled
	STEAMSHIM_EResultNeedCaptcha = 101,						   // Needs to provide a valid captcha
	STEAMSHIM_EResultGSLTDenied = 102,						   // a game server login token owned by this token's owner has been banned
	STEAMSHIM_EResultGSOwnerDenied = 103,					   // game server owner is denied for other reason (account lock, community ban, vac ban, missing phone)
	STEAMSHIM_EResultInvalidItemType = 104,					   // the type of thing we were requested to act on is invalid
	STEAMSHIM_EResultIPBanned = 105,						   // the ip address has been banned from taking this action
	STEAMSHIM_EResultGSLTExpired = 106,						   // this token has expired from disuse; can be reset for use
	STEAMSHIM_EResultInsufficientFunds = 107,				   // user doesn't have enough wallet funds to complete the action
	STEAMSHIM_EResultTooManyPending = 108,					   // There are too many of this thing pending already
	STEAMSHIM_EResultNoSiteLicensesFound = 109,				   // No site licenses found
	STEAMSHIM_EResultWGNetworkSendExceeded = 110,			   // the WG couldn't send a response because we exceeded max network send size
	STEAMSHIM_EResultAccountNotFriends = 111,				   // the user is not mutually friends
	STEAMSHIM_EResultLimitedUserAccount = 112,				   // the user is limited
	STEAMSHIM_EResultCantRemoveItem = 113,					   // item can't be removed
	STEAMSHIM_EResultAccountDeleted = 114,					   // account has been deleted
	STEAMSHIM_EResultExistingUserCancelledLicense = 115,	   // A license for this already exists, but cancelled
	STEAMSHIM_EResultCommunityCooldown = 116,				   // access is denied because of a community cooldown (probably from support profile data resets)
	STEAMSHIM_EResultNoLauncherSpecified = 117,				   // No launcher was specified, but a launcher was needed to choose correct realm for operation.
	STEAMSHIM_EResultMustAgreeToSSA = 118,					   // User must agree to china SSA or global SSA before login
	STEAMSHIM_EResultLauncherMigrated = 119,				   // The specified launcher type is no longer supported; the user should be directed elsewhere
	STEAMSHIM_EResultSteamRealmMismatch = 120,				   // The user's realm does not match the realm of the requested resource
	STEAMSHIM_EResultInvalidSignature = 121,				   // signature check did not match
	STEAMSHIM_EResultParseFailure = 122,					   // Failed to parse input
	STEAMSHIM_EResultNoVerifiedPhone = 123,					   // account does not have a verified phone number

};

enum connection_status_e {
	STEAMSHIM_ESteamNetworkingConnectionState_None = 0,
	STEAMSHIM_ESteamNetworkingConnectionState_Connecting = 1,
	STEAMSHIM_ESteamNetworkingConnectionState_FindingRoute = 2,
	STEAMSHIM_ESteamNetworkingConnectionState_Connected = 3,
	STEAMSHIM_ESteamNetworkingConnectionState_ClosedByPeer = 4,
	STEAMSHIM_ESteamNetworkingConnectionState_ProblemDetectedLocally = 5,
	STEAMSHIM_ESteamNetworkingConnectionState_FinWait = -1,
	STEAMSHIM_ESteamNetworkingConnectionState_Linger = -2,
	STEAMSHIM_ESteamNetworkingConnectionState_Dead = -3,
};

enum steam_cmd_s {
	RPC_BEGIN,
	RPC_PUMP = RPC_BEGIN,
	RPC_REQUEST_STEAM_ID,
	RPC_REQUEST_AVATAR,
	RPC_BEGIN_AUTH_SESSION,
	RPC_END_AUTH_SESSION,
	RPC_PERSONA_NAME,

	RPC_REQUEST_LAUNCH_COMMAND,

	RPC_ACTIVATE_OVERLAY,

	RPC_SET_RICH_PRESENCE,
	RPC_REFRESH_INTERNET_SERVERS,

	RPC_UPDATE_SERVERINFO,
	RPC_UPDATE_SERVERINFO_GAME_DATA,
	RPC_UPDATE_SERVERINFO_GAME_DESCRIPTION,
	RPC_UPDATE_SERVERINFO_GAME_TAGS,
	RPC_UPDATE_SERVERINFO_MAP_NAME,
	RPC_UPDATE_SERVERINFO_MOD_DIR,
	RPC_UPDATE_SERVERINFO_PRODUCT,
	RPC_UPDATE_SERVERINFO_REGION,
	RPC_UPDATE_SERVERINFO_SERVERNAME,

	RPC_CREATE_WORKSHOP_ITEM,

	RPC_SRV_P2P_LISTEN,
	RPC_SRV_P2P_ACCEPT_CONNECTION,
	RPC_SRV_P2P_DISCONNECT,
	RPC_SRV_P2P_SEND_MESSAGE,
	RPC_SRV_P2P_RECV_MESSAGES,
	RPC_SRV_P2P_CLOSE_LISTEN,

	RPC_P2P_CONNECT,
	RPC_P2P_DISCONNECT,

	RPC_P2P_SEND_MESSAGE,
	RPC_P2P_RECV_MESSAGES,

	RPC_AUTHSESSION_TICKET,

	RPC_GETVOICE,
	RPC_DECOMPRESS_VOICE,
	RPC_START_VOICE_RECORDING,
	RPC_STOP_VOICE_RECORDING,

	RPC_END,
	EVT_BEGIN = RPC_END,
	EVT_PERSONA_CHANGED = EVT_BEGIN,
	EVT_SRV_P2P_POLICY_RESPONSE,
	EVT_HEART_BEAT,
	EVT_GAME_JOIN,
	EVT_P2P_CONNECTION_CHANGED,

	EVT_SRV_P2P_CONNECTION_CHANGED,

	EVT_END,
	CMD_LEN
};
#define STEAM_EVT_LEN ( EVT_END - EVT_BEGIN )
#define STEAM_RPC_LEN ( RPC_END - RPC_BEGIN )

#define STEAM_SHIM_COMMON() enum steam_cmd_s cmd;
#define STEAM_RPC_REQ( name ) struct name##_req_s
#define STEAM_RPC_RECV( name ) struct name##_recv_s
#define STEAM_EVT( name ) struct name##_evt_s

#pragma pack( push, 1 )
struct steam_shim_common_s {
	STEAM_SHIM_COMMON()
};

#define STEAM_RPC_SHIM_COMMON() \
	STEAM_SHIM_COMMON()         \
	uint32_t sync;

struct steam_rpc_shim_common_s {
	STEAM_RPC_SHIM_COMMON()
};

struct steam_id_rpc_s {
	STEAM_RPC_SHIM_COMMON()
	uint64_t id;
};

struct buffer_rpc_s {
	STEAM_RPC_SHIM_COMMON()
	uint8_t buf[];
};
#define RPC_BUFFER_SIZE( buf, size ) ( size - sizeof( buf ) )

STEAM_RPC_REQ( server_info )
{
	STEAM_RPC_SHIM_COMMON()
	bool advertise;
	bool dedicated;
	int maxPlayerCount;
	int botPlayerCount;
};

STEAM_RPC_REQ( steam_auth )
{
	STEAM_RPC_SHIM_COMMON()
	uint64_t pcbTicket;
	char ticket[STEAM_AUTH_TICKET_MAXSIZE];
};

STEAM_RPC_REQ( steam_avatar )
{
	STEAM_RPC_SHIM_COMMON()
	uint64_t steamID;
	enum steam_avatar_size_e size;
};

STEAM_RPC_RECV( steam_avatar )
{
	STEAM_RPC_SHIM_COMMON()
	uint32_t width;
	uint32_t height;
	uint8_t buf[];
};

STEAM_RPC_RECV( auth_session_ticket )
{
	STEAM_RPC_SHIM_COMMON()
	uint32_t pcbTicket;
	char ticket[AUTH_TICKET_MAXSIZE];
};

STEAM_RPC_REQ( begin_auth_session )
{
	STEAM_RPC_SHIM_COMMON()
	uint64_t steamID;
	uint64_t cbAuthTicket;
	uint8_t authTicket[STEAM_AUTH_TICKET_MAXSIZE];
};

STEAM_RPC_RECV( steam_result )
{
	STEAM_RPC_SHIM_COMMON()
	int result;
};

STEAM_RPC_REQ( p2p_accept_connect )
{
	STEAM_RPC_SHIM_COMMON()
	uint32_t handle;
};

STEAM_RPC_RECV( p2p_accept_connection )
{
	STEAM_RPC_SHIM_COMMON()
	uint32_t result; // steam_result_e
};

STEAM_RPC_REQ( p2p_connect )
{
	STEAM_RPC_SHIM_COMMON()
	uint64_t steamID;
};

STEAM_RPC_RECV( p2p_connect )
{
	STEAM_RPC_SHIM_COMMON()
	uint32_t handle;
};

STEAM_RPC_REQ( p2p_disconnect )
{
	STEAM_RPC_SHIM_COMMON()
	uint32_t handle;
};

STEAM_RPC_REQ( p2p_listen )
{
	STEAM_RPC_SHIM_COMMON()
	uint64_t steamID;
};

STEAM_RPC_RECV( p2p_listen )
{
	STEAM_RPC_SHIM_COMMON()
	bool success;
	uint64_t steamID;
};

STEAM_RPC_REQ( send_message )
{
	STEAM_RPC_SHIM_COMMON()
	int messageReliability;
	uint32_t handle;
	int count;
	char buffer[];
};

STEAM_RPC_REQ( recv_messages )
{
	STEAM_RPC_SHIM_COMMON()
	uint32_t handle;
};

STEAM_RPC_RECV( recv_messages )
{
	STEAM_RPC_SHIM_COMMON()
	uint64_t steamID;
	uint32_t handle; // -1 for the client->server handle
	int count;
	struct {
		int count;
	} messageinfo[32];
	char buffer[];
};

STEAM_RPC_RECV( getvoice )
{
	STEAM_RPC_SHIM_COMMON()
	uint32_t count;
	uint8_t buffer[];
};

STEAM_RPC_REQ( decompress_voice )
{
	STEAM_RPC_SHIM_COMMON()
	uint32_t count;
	uint8_t buffer[];
};
STEAM_RPC_RECV( decompress_voice )
{
	STEAM_RPC_SHIM_COMMON()
	uint32_t count;
	uint8_t buffer[];
};

struct steam_rpc_pkt_s {
	union {
		struct steam_rpc_shim_common_s common;
		struct steam_auth_req_s steam_auth;
		struct steam_avatar_req_s avatar_req;
		struct steam_avatar_recv_s avatar_recv;
		struct steam_rpc_shim_common_s pump;
		struct begin_auth_session_req_s begin_auth_session;
		struct steam_id_rpc_s end_auth_session;
		struct buffer_rpc_s launch_command;
		struct buffer_rpc_s rich_presence;

		struct auth_session_ticket_recv_s auth_session;

		struct p2p_accept_connect_req_s p2p_accept_connection_req;
		struct p2p_accept_connection_recv_s p2p_accept_connection_recv;

		struct p2p_connect_req_s p2p_connect;
		struct p2p_disconnect_req_s p2p_disconnect;
		struct p2p_connect_req_s p2p_listen;
		struct p2p_connect_recv_s p2p_connect_recv;
		struct p2p_listen_recv_s p2p_listen_recv;

		struct send_message_req_s send_message;
		struct recv_messages_req_s recv_messages;
		struct recv_messages_recv_s recv_messages_recv;

		struct getvoice_recv_s getvoice_recv;
		struct decompress_voice_req_s decompress_voice;
		struct decompress_voice_recv_s decompress_voice_recv;

		struct buffer_rpc_s persona_name;

		struct steam_id_rpc_s open_overlay;
		struct steam_id_rpc_s steam_id;
		struct server_info_req_s server_info;
		struct buffer_rpc_s server_game_data;
		struct buffer_rpc_s server_description;
		struct buffer_rpc_s server_tags;
		struct buffer_rpc_s server_map_name;
		struct buffer_rpc_s server_mod_dir;
		struct buffer_rpc_s server_product;
		struct buffer_rpc_s server_region;
		struct buffer_rpc_s server_name;
	};
};

STEAM_EVT( policy_response )
{
	STEAM_SHIM_COMMON()
	uint64_t steamID;
	bool secure;
};

STEAM_EVT( p2p_connection_status_changed )
{
	STEAM_SHIM_COMMON()
	uint32_t steamNetHandle;
	uint64_t steamID;
	uint32_t StateChanged; // connection_status_e
};

STEAM_EVT( persona_changes )
{
	STEAM_SHIM_COMMON()
	uint64_t steamID;
	uint32_t avatar_changed : 1;
	// uint32_t name_change : 1;
	// uint32_t status_change: 1;
	// uint32_t online_status_change: 1;
};

STEAM_EVT( join_request )
{
	STEAM_SHIM_COMMON()
	uint64_t steamID;
	char rgchConnect[256];
};

STEAM_EVT( p2p_new_connection )
{
	STEAM_SHIM_COMMON()
	uint64_t steamID;
	uint32_t handle;
};

STEAM_EVT( p2p_lost_connection )
{
	STEAM_SHIM_COMMON()
	uint64_t steamID;
	uint32_t handle;
};

STEAM_EVT( p2p_net_connection_changed )
{
	STEAM_SHIM_COMMON()
	uint64_t identityRemoteSteamID;
	uint32_t listenSocket;
	uint32_t hConn;
	uint32_t oldState;
	uint32_t state;
};

struct steam_evt_pkt_s {
	union {
		struct steam_shim_common_s common;
		struct join_request_evt_s join_request;
		struct persona_changes_evt_s persona_changed;
		struct p2p_new_connection_evt_s p2p_new_connection;
		struct p2p_lost_connection_evt_s p2p_lost_connection;
		struct p2p_connection_status_changed_evt_s p2p_connection_status_changed;
		struct p2p_net_connection_changed_evt_s p2p_net_connection_changed;
		struct policy_response_evt_s policy_response; 
	};
};

#define STEAM_PACKED_RESERVE_SIZE ( 16384 )
struct steam_packet_buf {
	union {
		struct {
			uint32_t size;
			union {
				struct steam_shim_common_s common;
				struct steam_rpc_shim_common_s rpc_common;
				struct steam_rpc_pkt_s rpc_payload;
				struct steam_evt_pkt_s evt_payload;
			};
		};
		uint8_t buffer[STEAM_PACKED_RESERVE_SIZE];
	};
};

#pragma pack( pop )

typedef void ( *STEAMSHIM_rpc_handle )( void *self, struct steam_rpc_pkt_s *rec );
typedef void ( *STEAMSHIM_evt_handle )( void *self, struct steam_evt_pkt_s *rec );

#endif
