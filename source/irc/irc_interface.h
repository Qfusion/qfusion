#ifndef IRC_INTERFACE_H
#define IRC_INTERFACE_H

#include "../client/keys.h"
#include "../client/vid.h"
#include "../qcommon/dynvar.h"
#include "../qcommon/cvar.h"
#include "../qcommon/trie.h"

#define IRC_API_VERSION 3

// numeric commands as specified by RFC 1459 - Internet Relay Chat Protocol
typedef enum irc_numeric_e {

	// command replies
	RPL_WELCOME = 1,				// ":Welcome to the Internet Relay Network <nick>!<user>@<host>"
	RPL_YOURHOST = 2,				// ":Your host is <servername>, running version <ver>"
	RPL_CREATED = 3,				// ":This server was created <date>"
	RPL_MYINFO = 4,					// "<servername> <version> <available user modes> <available channel modes>"
	RPL_ISUPPORT = 5,				// "<nick> <parameter> * :are supported by this server"
	RPL_HELLO = 20,					// ":Please wait while we process your connection"
	RPL_NONE = 300,
	RPL_USERHOST = 302,				// ":[<reply>{<space><reply>}]"
	RPL_ISON = 303,					// ":[<nick> {<space><nick>}]"
	RPL_AWAY = 301,					// "<nick> :<away message>"
	RPL_UNAWAY = 305,				// ":You are no longer marked as being away"
	RPL_NOWAWAY = 306,				// ":You have been marked as being away"
	RPL_WHOISUSER = 311,			// "<nick> <user> <host> * :<real name>"
	RPL_WHOISSERVER = 312,			// "<nick> <server> :<server info>"
	RPL_WHOISOPERATOR = 313,		// "<nick> :is an IRC operator"
	RPL_WHOISIDLE = 317,			// "<nick> <integer> :seconds idle"
	RPL_ENDOFWHOIS = 318,			// "<nick> :End of /WHOIS list"
	RPL_WHOISCHANNELS = 319,		// "<nick> :{[@|+]<channel><space>}"
	RPL_WHOWASUSER = 314,			// "<nick> <user> <host> * :<real name>"
	RPL_ENDOFWHOWAS = 369,			// "<nick> :End of WHOWAS"
	RPL_WHOISACCOUNT = 330,			// "<nick> <account> :is logged in as"

	RPL_LISTSTART = 321,			// "Channel :Users  Name"
	RPL_LIST = 322,					// "<channel> <# visible> :<topic>"
	RPL_LISTEND = 323,				// ":End of /LIST"
	RPL_CHANNELMODEIS = 324,		// "<channel> <mode> <mode params>"
	RPL_NOTOPIC = 331,				// "<channel> :No topic is set"
	RPL_TOPIC = 332,				// "<channel> :<topic>"
	RPL_TOPICWHOTIME = 333,			// "<channel> <nick> <time>"
	RPL_INVITING = 341,				// "<channel> <nick>"
	RPL_SUMMONING = 342,			// "<user> :Summoning user to IRC"
	RPL_VERSION = 351,				// "<version>.<debuglevel> <server> :<comments>"
	RPL_WHOREPLY = 352,				// "<channel> <user> <host> <server> <nick> <H|G>[*][@|+] :<hopcount> <real name>"
	RPL_ENDOFWHO = 315,				// "<name> :End of /WHO list"
	RPL_NAMREPLY = 353,				// "<channel> :[[@|+]<nick> [[@|+]<nick> [...]]]"
	RPL_ENDOFNAMES = 366,			// "<channel> :End of /NAMES list"
	RPL_LINKS = 364,				// "<mask> <server> :<hopcount> <server info>"
	RPL_ENDOFLINKS = 365,			// "<mask> :End of /LINKS list"
	RPL_BANLIST = 367,				// "<channel> <banid>"
	RPL_ENDOFBANLIST = 368,			// "<channel> :End of channel ban list"
	RPL_INFO = 371,					// ":<string>"
	RPL_ENDOFINFO = 374,			// ":End of /INFO list"
	RPL_MOTDSTART = 375,			// ":- <server> Message of the day - "
	RPL_MOTD = 372,					// ":- <text>"
	RPL_ENDOFMOTD = 376,			// ":End of /MOTD command"
	RPL_YOUREOPER = 381,			// ":You are now an IRC operator"
	RPL_REHASHING = 382,			// "<config file> :Rehashing"
	RPL_TIME = 391,					// "<server> :<string showing server's local time>"
	RPL_USERSSTART = 392,			// ":UserID   Terminal  Host"
	RPL_USERS = 393,				// ":%-8s %-9s %-8s"
	RPL_ENDOFUSERS = 394,			// ":End of users"
	RPL_NOUSERS = 395,				// ":Nobody logged in"
	RPL_TRACELINK = 200,			// "Link <version & debug level> <destination> <next server>"
	RPL_TRACECONNECTING = 201,		// "Try. <class> <server>"
	RPL_TRACEHANDSHAKE = 202,		// "H.S. <class> <server>"
	RPL_TRACEUNKNOWN = 203,			// "???? <class> [<client IP address in dot form>]"
	RPL_TRACEOPERATOR = 204,		// "Oper <class> <nick>"
	RPL_TRACEUSER = 205,			// "User <class> <nick>"
	RPL_TRACESERVER = 206,			// "Serv <class> <int>S <int>C <server> <nick!user|*!*>@<host|server>"
	RPL_TRACENEWTYPE = 208,			// "<newtype> 0 <client name>"
	RPL_TRACELOG = 261,				// "File <logfile> <debug level>"
	RPL_STATSLINKINFO = 211,		// "<linkname> <sendq> <sent messages> <sent bytes> <received messages> <received bytes> <time open>"
	RPL_STATSCOMMANDS = 212,		// "<command> <count>"
	RPL_STATSCLINE = 213,			// "C <host> * <name> <port> <class>"
	RPL_STATSNLINE = 214,			// "N <host> * <name> <port> <class>"
	RPL_STATSILINE = 215,			// "I <host> * <host> <port> <class>"
	RPL_STATSKLINE = 216,			// "K <host> * <username> <port> <class>"
	RPL_STATSYLINE = 218,			// "Y <class> <ping frequency> <connectfrequency> <max sendq>"
	RPL_ENDOFSTATS = 219,			// "<stats letter> :End of /STATS report"
	RPL_STATSLLINE = 241,			// "L <hostmask> * <servername> <maxdepth>"
	RPL_STATSUPTIME = 242,			// ":Server Up %d days %d:%02d:%02d"
	RPL_STATSOLINE = 243,			// "O <hostmask> * <name>"
	RPL_STATSHLINE = 244,			// "H <hostmask> * <servername>"
	RPL_UMODEIS = 221,				// "<user mode string>"
	RPL_LUSERCLIENT = 251,			// ":There are <integer> users and <integer> invisible on <integer> servers"
	RPL_LUSEROP = 252,				// "<integer> :operator(s) online"
	RPL_LUSERUNKNOWN = 253,			// "<integer> :unknown connection(s)"
	RPL_LUSERCHANNELS = 254,		// "<integer> :channels formed"
	RPL_LUSERME = 255,				// ":I have <integer> clients and <integer> servers"
	RPL_ADMINME = 256,				// "<server> :Administrative info"
	RPL_ADMINLOC1 = 257,			// ":<admin info>"
	RPL_ADMINLOC2 = 258,			// ":<admin info>"
	RPL_ADMINEMAIL = 259,			// ":<admin info>"
	RPL_LOCALUSERS = 265,
	RPL_GLOBALUSERS = 266,

	// error codes
	ERR_NOSUCHNICK = 401,			// "<nickname> :No such nick/channel"
	ERR_NOSUCHSERVER = 402,			// "<server name> :No such server"
	ERR_NOSUCHCHANNEL = 403,		// "<channel name> :No such channel"
	ERR_CANNOTSENDTOCHAN = 404,		// "<channel name> :Cannot send to channel"
	ERR_TOOMANYCHANNELS = 405,		// "<channel name> :You have joined too many channels"
	ERR_WASNOSUCHNICK = 406,		// "<nickname> :There was no such nickname"
	ERR_TOOMANYTARGETS = 407,		// "<target> :Duplicate recipients. No message delivered"
	ERR_NOORIGIN = 409,				// ":No origin specified"
	ERR_NORECIPIENT = 411,			// ":No recipient given (<command>)"
	ERR_NOTEXTTOSEND = 412,			// ":No text to send"
	ERR_NOTOPLEVEL = 413,			// "<mask> :No toplevel domain specified"
	ERR_WILDTOPLEVEL = 414,			// "<mask> :Wildcard in toplevel domain"
	ERR_UNKNOWNCOMMAND = 421,		// "<command> :Unknown command"
	ERR_NOMOTD = 422,				// ":MOTD File is missing"
	ERR_NOADMININFO = 423,			// "<server> :No administrative info available"
	ERR_FILEERROR = 424,			// ":File error doing <file op> on <file>"
	ERR_NONICKNAMEGIVEN = 431,		// ":No nickname given"
	ERR_ERRONEUSNICKNAME = 432,		// "<nick> :Erroneus nickname"
	ERR_NICKNAMEINUSE = 433,		// "<nick> :Nickname is already in use"
	ERR_NICKCOLLISION = 436,		// "<nick> :Nickname collision KILL"
	ERR_BANNICKCHANGE = 437,
	ERR_NCHANGETOOFAST = 438,
	ERR_USERNOTINCHANNEL = 441,		// "<nick> <channel> :They aren't on that channel"
	ERR_NOTONCHANNEL = 442,			// "<channel> :You're not on that channel"
	ERR_USERONCHANNEL = 443,		// "<user> <channel> :is already on channel"
	ERR_NOLOGIN = 444,				// "<user> :User not logged in"
	ERR_SUMMONDISABLED = 445,		// ":SUMMON has been disabled"
	ERR_USERSDISABLED = 446,		// ":USERS has been disabled"
	ERR_NOTREGISTERED = 451,		// ":You have not registered"
	ERR_NEEDMOREPARAMS = 461,		// "<command> :Not enough parameters"
	ERR_ALREADYREGISTRED = 462,		// ":You may not reregister"
	ERR_NOPERMFORHOST = 463,		// ":Your host isn't among the privileged"
	ERR_PASSWDMISMATCH = 464,		// ":Password incorrect"
	ERR_YOUREBANNEDCREEP = 465,		// ":You are banned from this server"
	ERR_BADNAME = 468,				// ":Your username is invalid"
	ERR_KEYSET = 467,				// "<channel> :Channel key already set"
	ERR_CHANNELISFULL = 471,		// "<channel> :Cannot join channel (+l)"
	ERR_UNKNOWNMODE = 472,			// "<char> :is unknown mode char to me"
	ERR_INVITEONLYCHAN = 473,		// "<channel> :Cannot join channel (+i)"
	ERR_BANNEDFROMCHAN = 474,		// "<channel> :Cannot join channel (+b)"
	ERR_BADCHANNELKEY = 475,		// "<channel> :Cannot join channel (+k)"
	ERR_NOPRIVILEGES = 481,			// ":Permission Denied- You're not an IRC operator"
	ERR_CHANOPRIVSNEEDED = 482,		// "<channel> :You're not channel operator"
	ERR_CANTKILLSERVER = 483,		// ":You cant kill a server!"
	ERR_NOOPERHOST = 491,			// ":No O-lines for your host"
	ERR_UMODEUNKNOWNFLAG = 501,		// ":Unknown MODE flag"
	ERR_USERSDONTMATCH = 502,		// ":Cant change mode for other users"
	ERR_GHOSTEDCLIENT = 503,
	ERR_LAST_ERR_MSG = 504,
	ERR_SILELISTFULL = 511,
	ERR_NOSUCHGLINE = 512,
	ERR_TOOMANYWATCH = 512,
	ERR_BADPING = 513,
	ERR_TOOMANYDCC = 514,
	ERR_LISTSYNTAX = 521,
	ERR_WHOSYNTAX = 522,
	ERR_WHOLIMEXCEED = 523

} irc_numeric_t;

typedef enum irc_command_type_e {
	IRC_COMMAND_NUMERIC,
	IRC_COMMAND_STRING
} irc_command_type_t;

// commands can be numeric's or string
typedef struct irc_command_s {
	union {
		// tagged union
		irc_numeric_t	numeric;
		const char *	string;
	};
	irc_command_type_t	type;
} irc_command_t;

// listener signature
typedef void (*irc_listener_f)(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);

struct mufont_s;
struct shader_s;
struct poly_s;
struct irc_chat_history_node_s;

typedef struct
{
	// special messages
	void		(*Printf)(const char *msg);
	// client state
	int			(*CL_GetKeyDest)(void);
	connstate_t	(*CL_GetClientState)(void);
	// key destination control
	keydest_t	(*Key_DelegatePush)(key_delegate_f key_del, key_char_delegate_f char_del);
	void		(*Key_DelegatePop)(keydest_t next_dest);
	// drawing
	struct mufont_s *(*SCR_RegisterFont)(const char *name);
	void			(*SCR_DrawString)(int x, int y, int align, const char *str, struct mufont_s *font, vec4_t color);
	int				(*SCR_DrawStringWidth)(int x, int y, int align, const char *str, int maxwidth, struct mufont_s *font, vec4_t color);
	void			(*SCR_DrawRawChar)(int x, int y, qwchar num, struct mufont_s *font, vec4_t color);
	size_t			(*SCR_strHeight)(struct mufont_s *font );
	size_t			(*SCR_strWidth)(const char *str, struct mufont_s *font, int maxlen);
	size_t			(*SCR_StrlenForWidth)(const char *str, struct mufont_s *font, size_t maxwidth);
	struct shader_s*(*R_RegisterPic)(const char *name);
	void			(*R_DrawStretchPic)(int x, int y, int w, int h, float s1, float t1, float s2, float t2, float *color, struct shader_s *shader);
	void			(*R_DrawStretchPoly)(const struct poly_s *poly, float x_offset, float y_offset);
	viddef_t		*viddef;
	// clock
	unsigned int	(*Milliseconds)(void);
	quint64			(*Microseconds)(void);
	// managed memory allocation
	struct mempool_s *(*Mem_AllocPool)(const char *name, const char *filename, int fileline);	
	void		*(*Mem_Alloc)(int size, const char *filename, int fileline);
	void		(*Mem_Free)(void *data, const char *filename, int fileline);
	void		(*Mem_FreePool)(const char *filename, int fileline);
	void		(*Mem_EmptyPool)(const char *filename, int fileline);
	// dynvars
	dynvar_t	*(*Dynvar_Create)(const char *name, qboolean console, dynvar_getter_f getter, dynvar_setter_f setter);
	void		(*Dynvar_Destroy)(dynvar_t *dynvar);
	dynvar_t	*(*Dynvar_Lookup)(const char *name);
	const char	*(*Dynvar_GetName)(dynvar_t *dynvar);
	dynvar_get_status_t (*Dynvar_GetValue)(dynvar_t *dynvar,void **value);
	dynvar_set_status_t (*Dynvar_SetValue)(dynvar_t *dynvar,void *value);
	void		(*Dynvar_CallListeners)(dynvar_t *dynvar, void *value);
	void		(*Dynvar_AddListener)(dynvar_t *dynvar, dynvar_listener_f listener);
	void		(*Dynvar_RemoveListener)(dynvar_t *dynvar, dynvar_listener_f listener);
	dynvar_getter_f DYNVAR_WRITEONLY;
	dynvar_setter_f	DYNVAR_READONLY;
	// cvars
	cvar_t		*(*Cvar_Get)(const char *name, const char *value, int flags);
	cvar_t		*(*Cvar_Set)(const char *name, const char *value);
	void		(*Cvar_SetValue)(const char *name, float value);
	cvar_t		*(*Cvar_ForceSet)(const char *name, const char *value);
	int			(*Cvar_Integer)(const char *name);
	float		(*Cvar_Value)(const char *name);
	const char	*(*Cvar_String)(const char *name);
	// commands
	int			(*Cmd_Argc)(void);
	char		*(*Cmd_Argv)(int arg);
	char		*(*Cmd_Args)(void);
	void		(*Cmd_AddCommand)(const char *cmd_name, xcommand_t function);
	void		(*Cmd_RemoveCommand)(const char *cmd_name);
	void		(*Cmd_ExecuteString)(const char *text);
	void		(*Cmd_SetCompletionFunc)(const char *cmd_name,  xcompletionf_t completion_func);

	// console
	void		(*Com_BeginRedirect)(int target, char *buffer, int buffersize, void (*flush)(int, char*, const void*), const void *extra);
	void		(*Com_EndRedirect)(void);
	void		(*Cbuf_AddText)(const char *text);
	// tries
	trie_error_t (*Trie_Create)(trie_casing_t casing, struct trie_s **trie);
	trie_error_t (*Trie_Destroy)(struct trie_s *trie);
	trie_error_t (*Trie_Clear)(struct trie_s *trie);
	trie_error_t (*Trie_GetSize)(struct trie_s *trie, unsigned int *size);
	trie_error_t (*Trie_Insert)(struct trie_s *trie, const char *key, void *data);
	trie_error_t (*Trie_Remove)(struct trie_s *trie, const char *key, void **data);
	trie_error_t (*Trie_Replace)(struct trie_s *trie, const char *key, void *data_new, void **data_old);
	trie_error_t (*Trie_Find)(const struct trie_s *trie, const char *key, trie_find_mode_t mode, void **data);
	trie_error_t (*Trie_FindIf)(const struct trie_s *trie, const char *key, trie_find_mode_t mode, int (*predicate)(void *value, void *cookie), void *cookie, void **data);
	trie_error_t (*Trie_NoOfMatches)(const struct trie_s *trie, const char *prefix, unsigned int *matches);
	trie_error_t (*Trie_NoOfMatchesIf)(const struct trie_s *trie, const char *prefix, int (*predicate)(void *value, void *cookie), void *cookie, unsigned int *matches);
	trie_error_t (*Trie_Dump)(const struct trie_s *trie, const char *prefix, trie_dump_what_t what, struct trie_dump_s **dump);
	trie_error_t (*Trie_DumpIf)(const struct trie_s *trie, const char *prefix, trie_dump_what_t what, int (*predicate)(void *value, void *cookie), void *cookie, struct trie_dump_s **dump);
	trie_error_t (*Trie_FreeDump)(struct trie_dump_s *dump);
} irc_import_t;

typedef struct irc_export_s {
	int			(*API)(void);		// API version
	qboolean	(*Init)(void);
	void		(*Shutdown)(void);
	qboolean	(*Connect)(void);	// connects to irc_server:irc_port
	qboolean	(*Disconnect)(void);
	void		(*AddListener)(irc_command_t cmd, irc_listener_f listener);
	void		(*RemoveListener)(irc_command_t cmd, irc_listener_f listener);

	size_t		(*HistorySize)(void);
	size_t		(*HistoryTotalSize)(void);

	// history is in reverse order (newest line first)
	const struct irc_chat_history_node_s *(*GetHistoryHeadNode)(void);
	const struct irc_chat_history_node_s *(*GetNextHistoryNode)(const struct irc_chat_history_node_s *n);
	const struct irc_chat_history_node_s *(*GetPrevHistoryNode)(const struct irc_chat_history_node_s *n);
	const char *(*GetHistoryNodeLine)(const struct irc_chat_history_node_s *n);

	const char	*ERROR_MSG;			// error string (set after error)
} irc_export_t;

// the one and only function this shared library exports
typedef irc_export_t *(*GetIrcAPI_t)(const irc_import_t *imports);
QF_DLL_EXPORT irc_export_t *GetIrcAPI(const irc_import_t *imports);

#endif
