#include "irc_common.h"
#include "irc_logic.h"
#include "irc_listeners.h"
#include "irc_client.h"
#include "irc_rcon.h"

dynvar_t *irc_connected;
cvar_t *irc_server;
cvar_t *irc_port;
cvar_t *irc_nick;
cvar_t *irc_perform;
cvar_t *irc_defaultChannel;

// the functions in irc_export_t
int Irc_If_API(void);
qboolean Irc_If_Init(void);
void Irc_If_Shutdown(void);
qboolean Irc_If_Connect(void);
qboolean Irc_If_Disconnect(void);
size_t Irc_If_HistorySize(void);
size_t Irc_If_HistoryTotalSize(void);
const char *Irc_If_GetHistoryNodeLine(const irc_chat_history_node_t *n);
const irc_chat_history_node_t *Irc_If_GetHistoryHeadNode(void);
const irc_chat_history_node_t *Irc_If_GetNextHistoryNode(const irc_chat_history_node_t *n);
const irc_chat_history_node_t *Irc_If_GetPrevHistoryNode(const irc_chat_history_node_t *n);

irc_export_t *GetIrcAPI(const irc_import_t *imports) {
	static irc_export_t exports;
	IRC_IMPORT = *imports;
	exports.API = Irc_If_API;
	exports.Init = Irc_If_Init;
	exports.Shutdown = Irc_If_Shutdown;
	exports.Connect = Irc_If_Connect;
	exports.Disconnect = Irc_If_Disconnect;
	exports.AddListener = Irc_Proto_AddListener;
	exports.RemoveListener = Irc_Proto_RemoveListener;
	exports.HistorySize = Irc_If_HistorySize;
	exports.HistoryTotalSize = Irc_If_HistoryTotalSize;
	exports.GetHistoryHeadNode = Irc_If_GetHistoryHeadNode;
	exports.GetNextHistoryNode = Irc_If_GetNextHistoryNode;
	exports.GetPrevHistoryNode = Irc_If_GetPrevHistoryNode;
	exports.GetHistoryNodeLine = Irc_If_GetHistoryNodeLine;
	exports.ERROR_MSG = IRC_ERROR_MSG;
	return &exports;
}

int Irc_If_API(void) {
	return IRC_API_VERSION;
}

qboolean Irc_If_Init(void) {
	irc_connected = IRC_IMPORT.Dynvar_Lookup("irc_connected");
	irc_server = IRC_IMPORT.Cvar_Get("irc_server", "", 0);
	irc_port = IRC_IMPORT.Cvar_Get("irc_port", "", 0);
	irc_nick = IRC_IMPORT.Cvar_Get("irc_nick", "", 0);
	irc_perform = IRC_IMPORT.Cvar_Get("irc_perform", "exec irc_perform.cfg\n", 0);
	irc_defaultChannel = IRC_IMPORT.Cvar_Get("irc_defaultChannel", "", 0);
	assert(irc_connected);
	Irc_Proto_InitListeners();
	IRC_IMPORT.Dynvar_AddListener(irc_connected, Irc_Logic_Connected_f);
	IRC_IMPORT.Dynvar_AddListener(irc_connected, Irc_Client_Connected_f);
	IRC_IMPORT.Dynvar_AddListener(irc_connected, Irc_Rcon_Connected_f);
	return qtrue;
}

void Irc_If_Shutdown(void) {
	IRC_IMPORT.Dynvar_RemoveListener(irc_connected, Irc_Client_Connected_f);
	IRC_IMPORT.Dynvar_RemoveListener(irc_connected, Irc_Logic_Connected_f);
	IRC_IMPORT.Dynvar_RemoveListener(irc_connected, Irc_Rcon_Connected_f);
	Irc_Proto_TeardownListeners();					// remove remaining listeners (if any)
	Irc_ClearHistory();								// clear history buffer
}

size_t Irc_If_HistorySize(void) {
	return Irc_HistorySize();
}

size_t Irc_If_HistoryTotalSize(void) {
	return Irc_HistoryTotalSize();
}

const irc_chat_history_node_t *Irc_If_GetHistoryHeadNode(void) {
	return irc_chat_history;
}

const irc_chat_history_node_t *Irc_If_GetNextHistoryNode(const irc_chat_history_node_t *n) {
	return n ? n->next : NULL;
}

const irc_chat_history_node_t *Irc_If_GetPrevHistoryNode(const irc_chat_history_node_t *n) {
	return n ? n->prev : NULL;
}

const char *Irc_If_GetHistoryNodeLine(const irc_chat_history_node_t *n) {
	return n ? n->line : NULL;
}

qboolean Irc_If_Connect(void) {
	const char * const server = Cvar_GetStringValue(irc_server);
	const unsigned short port = Cvar_GetIntegerValue(irc_port);
	qboolean *c;
	Irc_Logic_Connect(server, port);						// try to connect
	IRC_IMPORT.Dynvar_GetValue(irc_connected, (void**) &c);	// get connection status
	return !*c;
}

qboolean Irc_If_Disconnect(void) {
	qboolean *c;
	IRC_IMPORT.Dynvar_GetValue(irc_connected, (void**) &c);	// get connection status
	Irc_Logic_Disconnect("");								// disconnect if connected
	return qfalse;											// always succeed
}
