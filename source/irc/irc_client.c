#include "irc_common.h"
#include "irc_client.h"
#include "irc_logic.h"
#include "irc_protocol.h"
#include "irc_listeners.h"

#define STRINGIFY(x) #x
#define DOUBLEQUOTE(x) STRINGIFY(x)

// externalized dynvars and cvars
cvar_t *irc_window = NULL;
cvar_t *irc_windowLines = NULL;
cvar_t *dedicated = NULL;

// warsow commands
static void Irc_Client_Join_f(void);
static void Irc_Client_Part_f(void);
static void Irc_Client_Msg_f(void);
static void Irc_Client_Action_f(void);
static void Irc_Client_PrivMsg_f(void);
static void Irc_Client_Mode_f(void);
static void Irc_Client_Topic_f(void);
static void Irc_Client_Names_f(void);
static void Irc_Client_Kick_f(void);
static void Irc_Client_Who_f(void);
static void Irc_Client_Whois_f(void);
static void Irc_Client_Whowas_f(void);
static void Irc_Client_Quote_f(void);
static void Irc_Client_Messagemode_f(void);
static void Irc_Client_Messagemode2_f(void);

// key event (for chat)
static void Irc_Client_KeyEvent_f(int key, bool *key_down);
static void Irc_Client_KeyEvent2_f(int key, bool *key_down);
static void Irc_Client_CharEvent_f(wchar_t key);

// dynvar listeners
static void Irc_Client_Draw_f(void *frametick);
static void Irc_Client_Frametick_f(void *frame);

// server command listeners
static void Irc_Client_NicknameInUse_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdError_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdNotice_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdEndofmotd_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdParamNotice_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdPrivmsg_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdTopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplNamreply_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplEndofnames_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplTopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplNotopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplWhoisuser_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplWhoisserver_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplWhoisoperator_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplWhoisidle_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplWhoischannels_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplWhoisaccount_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplEndofwhois_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplWhoreply_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplEndofwho_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplWhowasuser_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdRplEndofwhowas_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdMode_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdJoin_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdPart_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdQuit_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdKill_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdNick_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Client_CmdKick_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
#ifdef _DEBUG
static void Irc_Client_CmdGeneric_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
#endif

// chat buffer
#define IRC_MESSAGEMODE_BUFSIZE 256
enum {
	IRC_MESSAGEMODE_NONE = 0,
	IRC_MESSAGEMODE_CHANMSG,
	IRC_MESSAGEMODE_PRIVMSG_TARGET,
	IRC_MESSAGEMODE_PRIVMSG_TEXT
} static reading_from_keyboard = IRC_MESSAGEMODE_NONE;
static char	irc_messagemode_buf[IRC_MESSAGEMODE_BUFSIZE];
static int	irc_messagemode_buflen = 0;
static char	irc_messagemode_target_buf[256];
static int	irc_messagemode_target_buflen = 0;

void Irc_Client_Connected_f(void *connected) {

	bool *c = (bool *) connected;
	if (!dedicated)
		dedicated = IRC_IMPORT.Cvar_Get("dedicated", "1", CVAR_NOSET);

	if (*c) {

		// initialize
		irc_command_t cmd;
		cmd.type = IRC_COMMAND_STRING;
		cmd.string = "NOTICE";				Irc_Proto_AddListener(cmd, Irc_Client_CmdNotice_f);
		cmd.string = "PRIVMSG";				Irc_Proto_AddListener(cmd, Irc_Client_CmdPrivmsg_f);
		cmd.string = "MODE";				Irc_Proto_AddListener(cmd, Irc_Client_CmdMode_f);
		cmd.string = "JOIN";				Irc_Proto_AddListener(cmd, Irc_Client_CmdJoin_f);
		cmd.string = "PART";				Irc_Proto_AddListener(cmd, Irc_Client_CmdPart_f);
		cmd.string = "TOPIC";				Irc_Proto_AddListener(cmd, Irc_Client_CmdTopic_f);
		cmd.string = "NICK";				Irc_Proto_AddListener(cmd, Irc_Client_CmdNick_f);
		cmd.string = "QUIT";				Irc_Proto_AddListener(cmd, Irc_Client_CmdQuit_f);
		cmd.string = "KILL";				Irc_Proto_AddListener(cmd, Irc_Client_CmdKill_f);
		cmd.string = "KICK";				Irc_Proto_AddListener(cmd, Irc_Client_CmdKick_f);
		
		cmd.type = IRC_COMMAND_NUMERIC;
		cmd.numeric = RPL_HELLO;			Irc_Proto_AddListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_WELCOME;			Irc_Proto_AddListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_YOURHOST;			Irc_Proto_AddListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_CREATED;			Irc_Proto_AddListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_MYINFO;			Irc_Proto_AddListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_MOTDSTART;		Irc_Proto_AddListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_MOTD;				Irc_Proto_AddListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_ENDOFMOTD;		Irc_Proto_AddListener(cmd, Irc_Client_CmdEndofmotd_f);
		cmd.numeric = RPL_LOCALUSERS;		Irc_Proto_AddListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_GLOBALUSERS;		Irc_Proto_AddListener(cmd, Irc_Client_CmdNotice_f);

		cmd.numeric = RPL_ISUPPORT;			Irc_Proto_AddListener(cmd, Irc_Client_CmdParamNotice_f);
		cmd.numeric = RPL_LUSEROP;			Irc_Proto_AddListener(cmd, Irc_Client_CmdParamNotice_f);
		cmd.numeric = RPL_LUSERUNKNOWN;		Irc_Proto_AddListener(cmd, Irc_Client_CmdParamNotice_f);
		cmd.numeric = RPL_LUSERCHANNELS;	Irc_Proto_AddListener(cmd, Irc_Client_CmdParamNotice_f);
		cmd.numeric = RPL_LUSERCLIENT;		Irc_Proto_AddListener(cmd, Irc_Client_CmdParamNotice_f);
		cmd.numeric = RPL_LUSERME;			Irc_Proto_AddListener(cmd, Irc_Client_CmdParamNotice_f);

		cmd.numeric = RPL_NAMREPLY;			Irc_Proto_AddListener(cmd, Irc_Client_CmdRplNamreply_f);
		cmd.numeric = RPL_ENDOFNAMES;		Irc_Proto_AddListener(cmd, Irc_Client_CmdRplEndofnames_f);
		cmd.numeric = RPL_TOPIC;			Irc_Proto_AddListener(cmd, Irc_Client_CmdRplTopic_f);
		cmd.numeric = RPL_NOTOPIC;			Irc_Proto_AddListener(cmd, Irc_Client_CmdRplNotopic_f);

		cmd.numeric = RPL_WHOISUSER;		Irc_Proto_AddListener(cmd, Irc_Client_CmdRplWhoisuser_f);
		cmd.numeric = RPL_WHOISSERVER;		Irc_Proto_AddListener(cmd, Irc_Client_CmdRplWhoisserver_f);
		cmd.numeric = RPL_WHOISOPERATOR;	Irc_Proto_AddListener(cmd, Irc_Client_CmdRplWhoisoperator_f);
		cmd.numeric = RPL_WHOISIDLE;		Irc_Proto_AddListener(cmd, Irc_Client_CmdRplWhoisidle_f);
		cmd.numeric = RPL_WHOISCHANNELS;	Irc_Proto_AddListener(cmd, Irc_Client_CmdRplWhoischannels_f);
		cmd.numeric = RPL_WHOISACCOUNT;		Irc_Proto_AddListener(cmd, Irc_Client_CmdRplWhoisaccount_f);
		cmd.numeric = RPL_ENDOFWHOIS;		Irc_Proto_AddListener(cmd, Irc_Client_CmdRplEndofwhois_f);
		cmd.numeric = RPL_WHOREPLY;			Irc_Proto_AddListener(cmd, Irc_Client_CmdRplWhoreply_f);
		cmd.numeric = RPL_ENDOFWHO;			Irc_Proto_AddListener(cmd, Irc_Client_CmdRplEndofwho_f);
		cmd.numeric = RPL_WHOWASUSER;		Irc_Proto_AddListener(cmd, Irc_Client_CmdRplWhowasuser_f);
		cmd.numeric = RPL_ENDOFWHOWAS;		Irc_Proto_AddListener(cmd, Irc_Client_CmdRplEndofwhowas_f);

		// error codes
		cmd.numeric = ERR_NOSUCHNICK;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOSUCHSERVER;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOSUCHCHANNEL;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_CANNOTSENDTOCHAN;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_TOOMANYCHANNELS;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_WASNOSUCHNICK;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_TOOMANYTARGETS;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOORIGIN;			Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NORECIPIENT;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOTEXTTOSEND;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOTOPLEVEL;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_WILDTOPLEVEL;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_UNKNOWNCOMMAND;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOMOTD;			Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOADMININFO;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_FILEERROR;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NONICKNAMEGIVEN;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_ERRONEUSNICKNAME;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NICKNAMEINUSE;	Irc_Proto_AddListener(cmd, Irc_Client_NicknameInUse_f);
		cmd.numeric = ERR_NICKCOLLISION;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_BANNICKCHANGE;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NCHANGETOOFAST;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_USERNOTINCHANNEL;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOTONCHANNEL;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_USERONCHANNEL;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOLOGIN;			Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_SUMMONDISABLED;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_USERSDISABLED;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOTREGISTERED;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NEEDMOREPARAMS;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_ALREADYREGISTRED;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOPERMFORHOST;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_PASSWDMISMATCH;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_YOUREBANNEDCREEP;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_BADNAME;			Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_KEYSET;			Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_CHANNELISFULL;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_UNKNOWNMODE;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_INVITEONLYCHAN;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_BANNEDFROMCHAN;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_BADCHANNELKEY;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOPRIVILEGES;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_CHANOPRIVSNEEDED;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_CANTKILLSERVER;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOOPERHOST;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_UMODEUNKNOWNFLAG;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_USERSDONTMATCH;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_GHOSTEDCLIENT;	Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_LAST_ERR_MSG;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_SILELISTFULL;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOSUCHGLINE;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_TOOMANYWATCH;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_BADPING;			Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_TOOMANYDCC;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_LISTSYNTAX;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_WHOSYNTAX;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_WHOLIMEXCEED;		Irc_Proto_AddListener(cmd, Irc_Client_CmdError_f);

#ifdef _DEBUG
		Irc_Proto_AddGenericListener(Irc_Client_CmdGeneric_f);
#endif

		if (!Cvar_GetIntegerValue(dedicated)) {
			dynvar_t * const frametick = IRC_IMPORT.Dynvar_Lookup("frametick");
			IRC_IMPORT.Dynvar_AddListener(frametick, Irc_Client_Frametick_f);
			IRC_IMPORT.Cmd_AddCommand("irc_messagemode", Irc_Client_Messagemode_f);
			IRC_IMPORT.Cmd_AddCommand("irc_messagemode2", Irc_Client_Messagemode2_f);
		}
		IRC_IMPORT.Cmd_AddCommand("irc_join", Irc_Client_Join_f);
		IRC_IMPORT.Cmd_AddCommand("irc_part", Irc_Client_Part_f);
		IRC_IMPORT.Cmd_AddCommand("irc_privmsg", Irc_Client_PrivMsg_f);
		IRC_IMPORT.Cmd_AddCommand("irc_mode", Irc_Client_Mode_f);
		IRC_IMPORT.Cmd_AddCommand("irc_who", Irc_Client_Who_f);
		IRC_IMPORT.Cmd_AddCommand("irc_whois", Irc_Client_Whois_f);
		IRC_IMPORT.Cmd_AddCommand("irc_whowas", Irc_Client_Whowas_f);
		IRC_IMPORT.Cmd_AddCommand("irc_quote", Irc_Client_Quote_f);
		IRC_IMPORT.Cmd_AddCommand("irc_chanmsg", Irc_Client_Msg_f);
		IRC_IMPORT.Cmd_AddCommand("irc_action", Irc_Client_Action_f);
		IRC_IMPORT.Cmd_AddCommand("irc_topic", Irc_Client_Topic_f);
		IRC_IMPORT.Cmd_AddCommand("irc_names", Irc_Client_Names_f);
		IRC_IMPORT.Cmd_AddCommand("irc_kick", Irc_Client_Kick_f);

		Irc_Println( "Connected to %s.\n", IRC_COLOR_NONE, Cvar_GetStringValue( irc_server ) );

		srand( time( NULL ) );
	} else if (!*c) {

		// teardown
		irc_command_t cmd;
		cmd.type = IRC_COMMAND_STRING;
		cmd.string = "NOTICE";				Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNotice_f);
		cmd.string = "PRIVMSG";				Irc_Proto_RemoveListener(cmd, Irc_Client_CmdPrivmsg_f);
		cmd.string = "MODE";				Irc_Proto_RemoveListener(cmd, Irc_Client_CmdMode_f);
		cmd.string = "JOIN";				Irc_Proto_RemoveListener(cmd, Irc_Client_CmdJoin_f);
		cmd.string = "PART";				Irc_Proto_RemoveListener(cmd, Irc_Client_CmdPart_f);
		cmd.string = "TOPIC";				Irc_Proto_RemoveListener(cmd, Irc_Client_CmdTopic_f);
		cmd.string = "NICK";				Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNick_f);
		cmd.string = "QUIT";				Irc_Proto_RemoveListener(cmd, Irc_Client_CmdQuit_f);
		cmd.string = "KILL";				Irc_Proto_RemoveListener(cmd, Irc_Client_CmdKill_f);
		cmd.string = "KICK";				Irc_Proto_RemoveListener(cmd, Irc_Client_CmdKick_f);
		
		cmd.type = IRC_COMMAND_NUMERIC;
		cmd.numeric = RPL_HELLO;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_WELCOME;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_YOURHOST;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_CREATED;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_MYINFO;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_MOTDSTART;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_MOTD;				Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_ENDOFMOTD;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_LOCALUSERS;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNotice_f);
		cmd.numeric = RPL_GLOBALUSERS;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdNotice_f);

		cmd.numeric = RPL_ISUPPORT;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdParamNotice_f);
		cmd.numeric = RPL_LUSEROP;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdParamNotice_f);
		cmd.numeric = RPL_LUSERUNKNOWN;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdParamNotice_f);
		cmd.numeric = RPL_LUSERCHANNELS;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdParamNotice_f);
		cmd.numeric = RPL_LUSERCLIENT;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdParamNotice_f);
		cmd.numeric = RPL_LUSERME;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdParamNotice_f);

		cmd.numeric = RPL_NAMREPLY;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplNamreply_f);
		cmd.numeric = RPL_ENDOFNAMES;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplEndofnames_f);
		cmd.numeric = RPL_TOPIC;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplTopic_f);
		cmd.numeric = RPL_NOTOPIC;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplNotopic_f);

		cmd.numeric = RPL_WHOISUSER;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplWhoisuser_f);
		cmd.numeric = RPL_WHOISSERVER;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplWhoisserver_f);
		cmd.numeric = RPL_WHOISOPERATOR;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplWhoisoperator_f);
		cmd.numeric = RPL_WHOISIDLE;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplWhoisidle_f);
		cmd.numeric = RPL_WHOISCHANNELS;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplWhoischannels_f);
		cmd.numeric = RPL_WHOISACCOUNT;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplWhoisaccount_f);
		cmd.numeric = RPL_ENDOFWHOIS;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplEndofwhois_f);
		cmd.numeric = RPL_WHOREPLY;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplWhoreply_f);
		cmd.numeric = RPL_ENDOFWHO;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplEndofwho_f);
		cmd.numeric = RPL_WHOWASUSER;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplWhowasuser_f);
		cmd.numeric = RPL_ENDOFWHOWAS;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdRplEndofwhowas_f);

		cmd.numeric = ERR_NOSUCHNICK;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOSUCHSERVER;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOSUCHCHANNEL;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_CANNOTSENDTOCHAN;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_TOOMANYCHANNELS;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_WASNOSUCHNICK;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_TOOMANYTARGETS;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOORIGIN;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NORECIPIENT;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOTEXTTOSEND;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOTOPLEVEL;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_WILDTOPLEVEL;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_UNKNOWNCOMMAND;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOMOTD;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOADMININFO;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_FILEERROR;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NONICKNAMEGIVEN;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_ERRONEUSNICKNAME;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NICKNAMEINUSE;	Irc_Proto_RemoveListener(cmd, Irc_Client_NicknameInUse_f);
		cmd.numeric = ERR_NICKCOLLISION;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_BANNICKCHANGE;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NCHANGETOOFAST;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_USERNOTINCHANNEL;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOTONCHANNEL;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_USERONCHANNEL;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOLOGIN;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_SUMMONDISABLED;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_USERSDISABLED;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOTREGISTERED;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NEEDMOREPARAMS;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_ALREADYREGISTRED;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOPERMFORHOST;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_PASSWDMISMATCH;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_YOUREBANNEDCREEP;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_BADNAME;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_KEYSET;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_CHANNELISFULL;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_UNKNOWNMODE;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_INVITEONLYCHAN;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_BANNEDFROMCHAN;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_BADCHANNELKEY;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOPRIVILEGES;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_CHANOPRIVSNEEDED;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_CANTKILLSERVER;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOOPERHOST;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_UMODEUNKNOWNFLAG;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_USERSDONTMATCH;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_GHOSTEDCLIENT;	Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_LAST_ERR_MSG;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_SILELISTFULL;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_NOSUCHGLINE;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_TOOMANYWATCH;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_BADPING;			Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_TOOMANYDCC;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_LISTSYNTAX;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_WHOSYNTAX;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);
		cmd.numeric = ERR_WHOLIMEXCEED;		Irc_Proto_RemoveListener(cmd, Irc_Client_CmdError_f);

#ifdef _DEBUG
		Irc_Proto_RemoveGenericListener(Irc_Client_CmdGeneric_f);
#endif

		IRC_IMPORT.Cmd_RemoveCommand("irc_join");
		IRC_IMPORT.Cmd_RemoveCommand("irc_part");
		IRC_IMPORT.Cmd_RemoveCommand("irc_privmsg");
		IRC_IMPORT.Cmd_RemoveCommand("irc_mode");
		IRC_IMPORT.Cmd_RemoveCommand("irc_who");
		IRC_IMPORT.Cmd_RemoveCommand("irc_whois");
		IRC_IMPORT.Cmd_RemoveCommand("irc_whowas");
		IRC_IMPORT.Cmd_RemoveCommand("irc_quote");
		IRC_IMPORT.Cmd_RemoveCommand("irc_chanmsg");
		IRC_IMPORT.Cmd_RemoveCommand("irc_action");
		IRC_IMPORT.Cmd_RemoveCommand("irc_topic");
		IRC_IMPORT.Cmd_RemoveCommand("irc_names");
		IRC_IMPORT.Cmd_RemoveCommand("irc_kick");

		if (!Cvar_GetIntegerValue(dedicated)) {
			dynvar_t * const updatescreen = IRC_IMPORT.Dynvar_Lookup("updatescreen");
			assert(updatescreen);
			IRC_IMPORT.Dynvar_RemoveListener(updatescreen, Irc_Client_Draw_f);
			IRC_IMPORT.Cmd_RemoveCommand("irc_messagemode");
			IRC_IMPORT.Cmd_RemoveCommand("irc_messagemode2");
		}

		if (reading_from_keyboard != IRC_MESSAGEMODE_NONE) {
			IRC_IMPORT.Key_DelegatePop(key_game);
			reading_from_keyboard = IRC_MESSAGEMODE_NONE;
		}

		if( IRC_ERROR_MSG[0] )
			Irc_Println( "Disconnected from %s (%s).\n", IRC_COLOR_NONE, Cvar_GetStringValue( irc_server ), IRC_ERROR_MSG );
		else
			Irc_Println( "Disconnected from %s.\n", IRC_COLOR_NONE, Cvar_GetStringValue( irc_server ) );
	}
}

static void Irc_Client_Draw_f(void *frametick) {
	// called every single frame by updatescreen
	if (IRC_IMPORT.CL_GetClientState() == CA_ACTIVE) {
		// game is active
		if (IRC_IMPORT.CL_GetKeyDest() != key_console) {
			// not in console, we may draw
			if (!irc_window)
				irc_window = IRC_IMPORT.Cvar_Get("irc_window", "0", CVAR_ARCHIVE);
			if (!irc_windowLines)
				irc_windowLines = IRC_IMPORT.Cvar_Get("irc_windowLines", "8", CVAR_ARCHIVE);
			if (reading_from_keyboard) {
				const char *target = NULL, *buf = NULL;
				int buflen = 0;
				switch (reading_from_keyboard) {
					case IRC_MESSAGEMODE_CHANMSG:
						target = Cvar_GetStringValue(irc_defaultChannel);
						buf = irc_messagemode_buf;
						buflen = irc_messagemode_buflen;
						break;
					case IRC_MESSAGEMODE_PRIVMSG_TARGET:
						target = "Target";
						buf = irc_messagemode_target_buf;
						buflen = irc_messagemode_target_buflen;
						break;
					case IRC_MESSAGEMODE_PRIVMSG_TEXT:
						target = irc_messagemode_target_buf;
						buf = irc_messagemode_buf;
						buflen = irc_messagemode_buflen;
						break;
					default:
						assert(0);
						break;
				}
				Irc_Client_DrawNotify(target, buf, buflen);
			}
			if (Cvar_GetIntegerValue(irc_window) && Cvar_GetIntegerValue(irc_windowLines))
				Irc_Client_DrawIngameWindow();
		}
	} else if (reading_from_keyboard) {
		// game not active and still in messagemode, abort
		irc_messagemode_target_buflen = 0;
		irc_messagemode_target_buf[0] = '\0';
		irc_messagemode_buflen = 0;
		irc_messagemode_buf[0] = '\0';
		IRC_IMPORT.Key_DelegatePop(key_menu);
		reading_from_keyboard = IRC_MESSAGEMODE_NONE;
	}
}

static void Irc_Client_Frametick_f(void *frame) {
	dynvar_t * const updatescreen = IRC_IMPORT.Dynvar_Lookup("updatescreen");
	dynvar_t * const frametick = IRC_IMPORT.Dynvar_Lookup("frametick");
	assert(updatescreen);
	assert(frametick);
	IRC_IMPORT.Dynvar_AddListener(updatescreen, Irc_Client_Draw_f);
	IRC_IMPORT.Dynvar_RemoveListener(frametick, Irc_Client_Frametick_f);
}

static void Irc_Client_Join_f(void) {
	const int argc = IRC_IMPORT.Cmd_Argc();
	if (argc >= 2 && argc <= 3) {
		char * const channel = IRC_IMPORT.Cmd_Argv(1);
		char * const channel_pass = argc == 3	// password is optional
			? IRC_IMPORT.Cmd_Argv(2)
			: NULL;
		Irc_Proto_Join(channel, channel_pass);	// join desired channel
	} else
		Irc_Printf("usage: irc_join <channel> [<password>]\n");
}

static void Irc_Client_Part_f(void) {
	const int argc = IRC_IMPORT.Cmd_Argc();
	if (argc == 2) {
		char * const channel = IRC_IMPORT.Cmd_Argv(1);
		Irc_Proto_Part(channel);
	} else
		Irc_Printf("usage: irc_part <channel>\n");
}

static void Irc_Client_Msg_f(void) {
	if (IRC_IMPORT.Cmd_Argc() >= 2) {
		char cropped_msg[IRC_SEND_BUF_SIZE];
		char colored_msg[1024];
		const char *msg = IRC_IMPORT.Cmd_Args();
		const char * const nick = Cvar_GetStringValue(irc_nick);
		const char *channel;
		channel = Cvar_GetStringValue(irc_defaultChannel);
		if (*channel) {
			Q_strncpyz(cropped_msg, msg, sizeof(cropped_msg));
			Irc_ColorFilter(cropped_msg, IRC_COLOR_WSW_TO_IRC, colored_msg);
			Irc_Proto_Msg(channel, colored_msg);
			// create echo
			Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "<%s> %s", IRC_COLOR_IRC_TO_WSW, channel, nick, colored_msg);
		} else
			Irc_Printf("Join a channel first.\n");
	} else
		Irc_Printf("usage: irc_chanmsg {<msg>}\n");
}

static void Irc_Client_Action_f(void) {
	if (IRC_IMPORT.Cmd_Argc() >= 2) {
		char cropped_msg[IRC_SEND_BUF_SIZE];
		char colored_msg[1024];
		const char *msg = IRC_IMPORT.Cmd_Args();
		const char * const nick = Cvar_GetStringValue(irc_nick);
		const char *channel;
		channel = Cvar_GetStringValue(irc_defaultChannel);
		if (*channel) {
			Q_strncpyz(cropped_msg, msg, sizeof(cropped_msg) - 7);
			Irc_ColorFilter(cropped_msg, IRC_COLOR_WSW_TO_IRC, colored_msg);
			Q_strncpyz(cropped_msg, va( "%cACTION %s%c", IRC_CTCP_MARKER_CHR, colored_msg, IRC_CTCP_MARKER_CHR ), sizeof(cropped_msg));
			Irc_Proto_Msg(channel, cropped_msg);
			// create echo
			Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_MAGENTA "%s %s", IRC_COLOR_IRC_TO_WSW, channel, nick, colored_msg);
		} else
			Irc_Printf("Join a channel first.\n");
	} else
		Irc_Printf("usage: irc_action {<action>}\n");
}

static void Irc_Client_PrivMsg_f(void) {
	if (IRC_IMPORT.Cmd_Argc() >= 3) {
		char cropped_msg[IRC_SEND_BUF_SIZE];
		char colored_msg[1024];
		const char * const target = IRC_IMPORT.Cmd_Argv(1);
		const char * const format = Irc_IsChannel(target)
			? IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "<%s> %s"
			: IRC_COLOR_RED "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "<%s> %s";
		const char *msg = IRC_IMPORT.Cmd_Args() + strlen(target) + 1;
		Q_strncpyz(cropped_msg, msg, sizeof(cropped_msg));
		Irc_ColorFilter(cropped_msg, IRC_COLOR_WSW_TO_IRC, colored_msg);
		Irc_Proto_Msg(target, colored_msg);
		// create echo
		Irc_Println(format, IRC_COLOR_IRC_TO_WSW, target, Cvar_GetStringValue(irc_nick), colored_msg);
	} else
		Irc_Printf("usage: irc_privmsg <target> {<msg>}\n");
}

static void Irc_Client_Mode_f(void) {
	const int argc = IRC_IMPORT.Cmd_Argc();
	if (argc >= 3) {
		const char * const target = IRC_IMPORT.Cmd_Argv(1);
		const char * const modes = IRC_IMPORT.Cmd_Argv(2);
		const char * const params = argc >= 4
			? IRC_IMPORT.Cmd_Args() + strlen(target) + strlen(modes) + 2
			: NULL;
		Irc_Proto_Mode(target, modes, params);
	} else
		Irc_Printf("usage: irc_mode <target> <modes> {<param>}\n");
}

static void Irc_Client_Topic_f(void) {
	const int argc = IRC_IMPORT.Cmd_Argc();
	if (argc >= 2) {
		char * const channel = IRC_IMPORT.Cmd_Argv(1);
		irc_channel_t *chan = Irc_Logic_GetChannel(channel);
		if (chan) {
			if (argc >= 3) {
				char buf[1024];
				const char *in = IRC_IMPORT.Cmd_Args();
				char *out = buf;
				if (*in == '"')
					in += 2;
				in += strlen(channel) + 1;
				Irc_ColorFilter(in, IRC_COLOR_WSW_TO_IRC, out);
				if (*out == '"') {
					size_t out_len;
					++out;
					out_len = strlen(out);
					assert(out_len >= 1);
					out[out_len - 1] = '\0';
				}
				Irc_Proto_Topic(channel, out);
			} else
				Irc_Printf("%s topic: \"%s\"\n", channel, Irc_Logic_GetChannelTopic(chan));
		} else
			Irc_Printf("Not joined: %s\n", channel);
	} else
		Irc_Printf("usage: irc_topic <channel> [<topic>]\n");
}

static void Irc_Client_Names_f(void) {
	const int argc = IRC_IMPORT.Cmd_Argc();
	if (argc == 2) {
		char * const channel = IRC_IMPORT.Cmd_Argv(1);
		irc_channel_t *chan = Irc_Logic_GetChannel(channel);
		if (chan) {
			const trie_t * const names = Irc_Logic_GetChannelNames(chan);
			trie_dump_t *dump;
			unsigned int namebufsize = 1;
			unsigned int i;
			char *namebuf;
			char *out;
			IRC_IMPORT.Trie_Dump(names, "", TRIE_DUMP_BOTH, &dump);
			for (i = 0; i < dump->size; ++i)
				namebufsize += strlen(dump->key_value_vector[i].key) + 2;
			out = namebuf = Irc_MemAlloc(namebufsize);
			for (i = 0; i < dump->size; ++i) {
				const char *in;
				const irc_nick_prefix_t * const p = (irc_nick_prefix_t*) dump->key_value_vector[i].value;
				if (*p != IRC_NICK_PREFIX_NONE)
					*out++ = *p;
				for (in = (const char*) dump->key_value_vector[i].key; *in; ++in)
					*out++ = *in;
				if (i < dump->size - 1)
					*out++ = ' ';
			}
			*out++ = '\0';
			Irc_Println("%s names: \"%s\"\n", IRC_COLOR_IRC_TO_WSW, channel, namebuf);
			Irc_MemFree(namebuf);
			IRC_IMPORT.Trie_FreeDump(dump);
		} else
			Irc_Printf("Not joined: %s\n", channel);
	} else
		Irc_Printf("usage: irc_names <channel>\n");
}

static void Irc_Client_Kick_f(void) {
	const int argc = IRC_IMPORT.Cmd_Argc();
	if (argc >= 3) {
		char *channel = IRC_IMPORT.Cmd_Argv(1);
		if (Irc_Logic_GetChannel(channel)) {
			char colored_reason[1024];
			const char * const nick = IRC_IMPORT.Cmd_Argv(2);
			const char *reason;
			if (argc >= 4) {
				Irc_ColorFilter(IRC_IMPORT.Cmd_Args() + strlen(nick) + strlen(channel) + 2, IRC_COLOR_WSW_TO_IRC, colored_reason);
				reason = colored_reason;
			} else
				reason = NULL;
			Irc_Proto_Kick(channel, nick, reason);
		} else
			Irc_Printf("Not joined: %s.", channel);
	} else
		Irc_Printf("usage: irc_kick <channel> <nick> [<reason>]\n");
}

static void Irc_Client_Who_f(void) {
	if (IRC_IMPORT.Cmd_Argc() == 2) {
		Irc_Proto_Who(IRC_IMPORT.Cmd_Argv(1));
	} else
		Irc_Printf("usage: irc_who <usermask>");
}

static void Irc_Client_Whois_f(void) {
	if (IRC_IMPORT.Cmd_Argc() == 2) {
		Irc_Proto_Whois(IRC_IMPORT.Cmd_Argv(1));
	} else
		Irc_Printf("usage: irc_whois <nick>");
}

static void Irc_Client_Whowas_f(void) {
	if (IRC_IMPORT.Cmd_Argc() == 2) {
		Irc_Proto_Whowas(IRC_IMPORT.Cmd_Argv(1));
	} else
		Irc_Printf("usage: irc_whowas <nick>");
}

static void Irc_Client_Quote_f(void) {
	Irc_Proto_Quote(IRC_IMPORT.Cmd_Args());
}

static void Irc_Client_Messagemode_f(void) {
	if (!reading_from_keyboard && IRC_IMPORT.CL_GetClientState() == CA_ACTIVE) {
		reading_from_keyboard = IRC_MESSAGEMODE_CHANMSG;
		IRC_IMPORT.Key_DelegatePush(Irc_Client_KeyEvent_f, Irc_Client_CharEvent_f);
	}
}

static void Irc_Client_Messagemode2_f(void) {
	if (!reading_from_keyboard && IRC_IMPORT.CL_GetClientState() == CA_ACTIVE) {
		reading_from_keyboard = IRC_MESSAGEMODE_PRIVMSG_TARGET;
		IRC_IMPORT.Key_DelegatePush(Irc_Client_KeyEvent2_f, Irc_Client_CharEvent_f);
	}
}

static void Irc_Client_KeyEvent_f(int key, bool *key_down) {
	assert(reading_from_keyboard == IRC_MESSAGEMODE_CHANMSG);
	switch (key) {
		case K_ENTER:
		case KP_ENTER:
			if (irc_messagemode_buflen > 0) {
				IRC_IMPORT.Cbuf_AddText("irc_chanmsg \"");
				IRC_IMPORT.Cbuf_AddText(irc_messagemode_buf);
				IRC_IMPORT.Cbuf_AddText("\"\n");
				irc_messagemode_buflen = 0;
				irc_messagemode_buf[0] = '\0';
			}
			IRC_IMPORT.Key_DelegatePop(key_game);
			reading_from_keyboard = IRC_MESSAGEMODE_NONE;
			break;
		case K_BACKSPACE:
			if (irc_messagemode_buflen) {
				--irc_messagemode_buflen;
				irc_messagemode_buf[irc_messagemode_buflen] = '\0';
			}
			break;
		case K_ESCAPE:
			irc_messagemode_buflen = 0;
			irc_messagemode_buf[0] = '\0';
			IRC_IMPORT.Key_DelegatePop(key_game);
			reading_from_keyboard = IRC_MESSAGEMODE_NONE;
			break;
		case 12:
			irc_messagemode_buflen = 0;
			irc_messagemode_buf[0] = '\0';
			break;
	}
}

static void Irc_Client_KeyEvent2_f(int key, bool *key_down) {
	switch (reading_from_keyboard) {
		case IRC_MESSAGEMODE_PRIVMSG_TARGET:
			switch (key) {
				case K_ENTER:
				case KP_ENTER:
					if (irc_messagemode_target_buflen > 0) {
						// valid target, switch to text input
						reading_from_keyboard = IRC_MESSAGEMODE_PRIVMSG_TEXT;
					} else {
						// zero-length nick, abort
						IRC_IMPORT.Key_DelegatePop(key_game);
						reading_from_keyboard = IRC_MESSAGEMODE_NONE;
					}
					break;
				case K_BACKSPACE:
					if (irc_messagemode_target_buflen) {
						--irc_messagemode_target_buflen;
						irc_messagemode_target_buf[irc_messagemode_target_buflen] = '\0';
					}
					break;
				case K_ESCAPE:
					irc_messagemode_target_buflen = 0;
					irc_messagemode_target_buf[0] = '\0';
					IRC_IMPORT.Key_DelegatePop(key_game);
					reading_from_keyboard = IRC_MESSAGEMODE_NONE;
					break;
				case 12:
					irc_messagemode_target_buflen = 0;
					irc_messagemode_target_buf[0] = '\0';
					break;
			}
			break;
		case IRC_MESSAGEMODE_PRIVMSG_TEXT:
			switch (key) {
				case K_ENTER:
				case KP_ENTER:
					if (irc_messagemode_buflen > 0) {
						IRC_IMPORT.Cbuf_AddText("irc_privmsg ");
						IRC_IMPORT.Cbuf_AddText(irc_messagemode_target_buf);
						IRC_IMPORT.Cbuf_AddText(" \"");
						IRC_IMPORT.Cbuf_AddText(irc_messagemode_buf);
						IRC_IMPORT.Cbuf_AddText("\"\n");
						irc_messagemode_buflen = 0;
						irc_messagemode_buf[0] = '\0';
					}
					IRC_IMPORT.Key_DelegatePop(key_game);
					reading_from_keyboard = IRC_MESSAGEMODE_NONE;
					break;
				case K_BACKSPACE:
					if (irc_messagemode_buflen) {
						--irc_messagemode_buflen;
						irc_messagemode_buf[irc_messagemode_buflen] = '\0';
					}
					break;
				case K_ESCAPE:
					irc_messagemode_buflen = 0;
					irc_messagemode_buf[0] = '\0';
					IRC_IMPORT.Key_DelegatePop(key_game);
					reading_from_keyboard = IRC_MESSAGEMODE_NONE;
					break;
				case 12:
					irc_messagemode_buflen = 0;
					irc_messagemode_buf[0] = '\0';
					break;
			}
			break;
		default:
			assert(0);
			break;
	}
}

static void Irc_Client_CharEvent_f(wchar_t key) {
	char *buf = NULL;
	int *buflen = NULL;
	switch (reading_from_keyboard) {
		case IRC_MESSAGEMODE_CHANMSG:
		case IRC_MESSAGEMODE_PRIVMSG_TEXT:
			buf = irc_messagemode_buf;
			buflen = &irc_messagemode_buflen;
			break;
		case IRC_MESSAGEMODE_PRIVMSG_TARGET:
			if (key == ' ')
				return;	// no space in nicks allowed
			buf = irc_messagemode_target_buf;
			buflen = &irc_messagemode_target_buflen;
			break;
		default:
			assert(0);
			break;
	}
	if (key >= 32 && key <= 126 && *buflen + 1 < IRC_MESSAGEMODE_BUFSIZE) {
		buf[(*buflen)++] = key;
		buf[*buflen] = '\0';
	}
}

static void Irc_Client_NicknameInUse_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char * const nick = Cvar_GetStringValue(irc_nick);
	assert(nick);
	if (!strncmp(nick, params, strlen(nick))) {
		const char * const space = strchr(params, ' ');
		if (space)
			params = space + 1;
	}
	Irc_Println(IRC_COLOR_RED "%s : %s", IRC_COLOR_IRC_TO_WSW, params, trailing);

	IRC_IMPORT.Cvar_ForceSet(irc_nick->name, va("%s_%04i", irc_nick->string, rand() % 9999));
	Irc_Proto_Nick(Cvar_GetStringValue(irc_nick));
}

static void Irc_Client_CmdError_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char * const nick = Cvar_GetStringValue(irc_nick);
	assert(nick);
	if (!strncmp(nick, params, strlen(nick))) {
		const char * const space = strchr(params, ' ');
		if (space)
			params = space + 1;
	}
	Irc_Println(IRC_COLOR_RED "%s : %s", IRC_COLOR_IRC_TO_WSW, params, trailing);
}

static void Irc_Client_CmdNotice_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	Irc_Println(IRC_COLOR_WHITE " %s", IRC_COLOR_IRC_TO_WSW, trailing);
}

static void Irc_Client_CmdEndofmotd_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	Irc_Client_CmdNotice_f(cmd, prefix, params, trailing);
	IRC_IMPORT.Cmd_ExecuteString("vstr irc_perform");
}

static void Irc_Client_CmdParamNotice_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *p = strchr(params, ' ');	// skip first param (nick)
	if (p) {
		++p;
		Irc_Println(IRC_COLOR_WHITE " %s %s", IRC_COLOR_IRC_TO_WSW, p, trailing);
	} else
		Irc_Println(IRC_COLOR_WHITE " %s", IRC_COLOR_IRC_TO_WSW, trailing);
	
}

static void Irc_Client_CmdPrivmsg_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char nick[IRC_SEND_BUF_SIZE];
	char * const emph = strchr(prefix, '!');
	memset(nick, 0, sizeof(nick));
	if (emph)
		memcpy(nick, prefix, emph - prefix);
	else
		strcpy(nick, prefix);
	if (Irc_IsChannel(params)) {
		// is private message
		if (*trailing == IRC_CTCP_MARKER_CHR) {
			// is probably a CTCP message
			if (!strncmp(trailing + 1, "ACTION ", 7)) {
				Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_MAGENTA "%s %s", IRC_COLOR_IRC_TO_WSW, params, nick, trailing + 7);
				return;
			}
		}

		Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "<%s> %s", IRC_COLOR_IRC_TO_WSW, params, nick, trailing);
	}
	else
		Irc_Println(IRC_COLOR_RED "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "<%s> %s", IRC_COLOR_IRC_TO_WSW, nick, nick, trailing);
}

static void Irc_Client_CmdTopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "%s sets topic: \"%s\"", IRC_COLOR_IRC_TO_WSW, params, nick, trailing);
}

static void Irc_Client_CmdRplNamreply_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *chan = strchr(params, ' ');
	if (chan) {
		irc_channel_t *channel;
		++chan;
		channel = Irc_Logic_GetChannel(chan);
		if (channel) {
			const trie_t * const names = Irc_Logic_GetChannelNames(channel);
			trie_dump_t *dump;
			unsigned int bufsize = 1;
			unsigned int i;
			char *allnames;
			char *out;
			IRC_IMPORT.Trie_Dump(names, "", TRIE_DUMP_BOTH, &dump);
			for (i = 0; i < dump->size; ++i)
				bufsize += strlen(dump->key_value_vector[i].key) + 2;
			out = allnames = Irc_MemAlloc(bufsize);
			for (i = 0; i < dump->size; ++i) {
				const char *in;
				const irc_nick_prefix_t * const p = (irc_nick_prefix_t*) dump->key_value_vector[i].value;
				if (*p != IRC_NICK_PREFIX_NONE)
					*out++ = *p;
				for (in = (const char*) dump->key_value_vector[i].key; *in; ++in)
					*out++ = *in;
				if (i < dump->size - 1)
					*out++ = ' ';
			}
			*out++ = '\0';
			Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "Names: %s", IRC_COLOR_IRC_TO_WSW, chan, allnames);
			Irc_MemFree(allnames);
			IRC_IMPORT.Trie_FreeDump(dump);
		}
	}
}

static void Irc_Client_CmdRplEndofnames_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *chan = strchr(params, ' ');
	if (chan) {
		irc_channel_t *channel;
		++chan;
		channel = Irc_Logic_GetChannel(chan);
		if (channel) {
			const trie_t * const names = Irc_Logic_GetChannelNames(channel);
			trie_dump_t *dump;
			unsigned int namebufsize = 1;
			unsigned int i;
			char *namebuf;
			char *out;
			IRC_IMPORT.Trie_Dump(names, "", TRIE_DUMP_BOTH, &dump);
			for (i = 0; i < dump->size; ++i)
				namebufsize += strlen(dump->key_value_vector[i].key) + 2;
			out = namebuf = Irc_MemAlloc(namebufsize);
			for (i = 0; i < dump->size; ++i) {
				const char *in;
				const irc_nick_prefix_t * const p = (irc_nick_prefix_t*) dump->key_value_vector[i].value;
				if (*p != IRC_NICK_PREFIX_NONE)
					*out++ = *p;
				for (in = (const char*) dump->key_value_vector[i].key; *in; ++in)
					*out++ = *in;
				if (i < dump->size - 1)
					*out++ = ' ';
			}
			*out++ = '\0';
			Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "Names: %s", IRC_COLOR_IRC_TO_WSW, chan, namebuf);
			Irc_MemFree(namebuf);
			IRC_IMPORT.Trie_FreeDump(dump);
		}
	}
}

static void Irc_Client_CmdRplTopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *chan = strchr(params, ' ');
	if (chan) {
		++chan;
		Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "Topic is: \"%s\"", IRC_COLOR_IRC_TO_WSW, chan, trailing);
	}
}

static void Irc_Client_CmdRplNotopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {	
	const char *chan = strchr(params, ' ');
	if (chan) {
		++chan;
		Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "No topic set.", IRC_COLOR_IRC_TO_WSW, chan);
	}
}

static void Irc_Client_CmdRplWhoisuser_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char buf[IRC_SEND_BUF_SIZE];
	const char *nick = "", *user = "", *host = "", *real_name = trailing;
	char *p;
	unsigned int i = 0;
	
	// parse params "<nick> <user> <host> * :<real name>"
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 1:
				nick = p;
				break;
			case 2:
				user = p;
				break;
			case 3:
				host = p;
				break;
		}
	}
	Irc_Println(IRC_COLOR_WHITE "%s is %s@%s : %s", IRC_COLOR_IRC_TO_WSW, nick, user, host, real_name);
}

static void Irc_Client_CmdRplWhoisserver_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char buf[IRC_SEND_BUF_SIZE];
	const char *nick = "", *server = "", *server_info = trailing;
	char *p;
	unsigned int i = 0;
	
	// parse params "<nick> <server> :<server info>"
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 1:
				nick = p;
				break;
			case 2:
				server = p;
				break;
		}
	}
	Irc_Println(IRC_COLOR_WHITE "%s using %s : %s", IRC_COLOR_IRC_TO_WSW, nick, server, server_info);
}

static void Irc_Client_CmdRplWhoisoperator_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *nick = strchr(params, ' ');
	if (nick) {
		++nick;
		Irc_Println(IRC_COLOR_WHITE "%s %s", IRC_COLOR_IRC_TO_WSW, nick, trailing);
	}
}

static void Irc_Client_CmdRplWhoischannels_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *nick = strchr(params, ' ');
	if (nick) {
		++nick;
		Irc_Println(IRC_COLOR_WHITE "%s on %s", IRC_COLOR_IRC_TO_WSW, nick, trailing);
	}
}

static void Irc_Client_CmdRplWhoisaccount_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char buf[IRC_SEND_BUF_SIZE];
	const char *nick = "", *account = "";
	char *p;
	unsigned int i = 0;
	
	// parse params "<nick> <account> :is logged in as"
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 1:
				nick = p;
				break;
			case 2:
				account = p;
				break;
		}
	}
	Irc_Println(IRC_COLOR_WHITE "%s %s %s", IRC_COLOR_IRC_TO_WSW, nick, trailing, account);
}

static void Irc_Client_CmdRplWhoisidle_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char buf[IRC_SEND_BUF_SIZE];
	const char *nick = "", *idle = "";
	char *p;
	unsigned int i = 0;

	// parse params "<nick> <integer> :seconds idle"
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 1:
				nick = p;
				break;
			case 2:
				idle = p;
				break;
		}
	}
	Irc_Println(IRC_COLOR_WHITE "%s is %s %s", IRC_COLOR_IRC_TO_WSW, nick, idle, trailing);
}

static void Irc_Client_CmdRplEndofwhois_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *nick = strchr(params, ' ');
	if (nick) {
		++nick;
		Irc_Println(IRC_COLOR_WHITE "%s %s", IRC_COLOR_IRC_TO_WSW, nick, trailing);
	}
}

static void Irc_Client_CmdRplWhoreply_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char buf[IRC_SEND_BUF_SIZE];
	const char *channel = "", *user = "", *host = "", *server = "", *nick = "", *hg = "";
	char *p;
	unsigned int i = 0;

	// parse params "<channel> <user> <host> <server> <nick> <H|G>[*][@|+] :<hopcount> <real name>"
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 0:
				channel = p;
				break;
			case 1:
				user = p;
				break;
			case 2:
				host = p;
				break;
			case 3:
				server = p;
				break;
			case 4:
				nick = p;
				break;
			case 5:
				hg = p;
				break;
		}
	}
	Irc_Println(IRC_COLOR_WHITE "%s %s %s %s %s %s : %s", IRC_COLOR_IRC_TO_WSW, channel, user, host, server, nick, hg, trailing);
}

static void Irc_Client_CmdRplEndofwho_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *name = strchr(params, ' ');
	if (name) {
		++name;
		Irc_Println(IRC_COLOR_WHITE "%s %s", IRC_COLOR_IRC_TO_WSW, name, trailing);
	}
}

static void Irc_Client_CmdRplWhowasuser_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char buf[IRC_SEND_BUF_SIZE];
	const char *nick = "", *user = "", *host = "", *real_name = trailing;
	char *p;
	unsigned int i = 0;

	// parse params "<nick> <user> <host> * :<real name>"
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		switch (i) {
			case 1:
				nick = p;
				break;
			case 2:
				user = p;
				break;
			case 3:
				host = p;
				break;
		}
	}
	Irc_Println(IRC_COLOR_WHITE "%s was %s@%s : %s", IRC_COLOR_IRC_TO_WSW, nick, user, host, real_name);
}

static void Irc_Client_CmdRplEndofwhowas_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *nick = strchr(params, ' ');
	if (nick) {
		++nick;
		Irc_Println(IRC_COLOR_WHITE "%s %s", IRC_COLOR_IRC_TO_WSW, nick, trailing);
	}
}

static void Irc_Client_CmdMode_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t pfx;
	Irc_ParseName(prefix, nick, &pfx);
	Irc_Println(IRC_COLOR_WHITE "%s sets mode %s", IRC_COLOR_IRC_TO_WSW, nick, params);
}

static void Irc_Client_CmdJoin_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	const char *chan = params[0]
		? params
		: trailing;
	Irc_ParseName(prefix, nick, &p);
	Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "Joins: %s (%s)", IRC_COLOR_IRC_TO_WSW, chan, nick, prefix);
}

static void Irc_Client_CmdPart_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "Parts: %s (%s)", IRC_COLOR_IRC_TO_WSW, params, nick, prefix);
}

static void Irc_Client_CmdQuit_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	Irc_Println(IRC_COLOR_GREEN "Quits: %s (%s)", IRC_COLOR_IRC_TO_WSW, nick, trailing);
}

static void Irc_Client_CmdKill_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	Irc_Println(IRC_COLOR_GREEN "Killed: %s (%s)", IRC_COLOR_IRC_TO_WSW, nick, trailing);
}

static void Irc_Client_CmdKick_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char buf[IRC_SEND_BUF_SIZE];
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	const char *chan, *victim;
	Irc_ParseName(prefix, nick, &p);
	strcpy(buf, params);
	chan = strtok(buf, " ");
	victim = strtok(NULL, " ");
	if (!strcmp(victim, Cvar_GetStringValue(irc_nick))) {
		// we have been kicked
		Irc_Println(IRC_COLOR_RED "You were kicked from %s by %s (%s)", IRC_COLOR_IRC_TO_WSW, chan, nick, trailing);
	} else {
		// someone else was kicked
		Irc_Println(IRC_COLOR_YELLOW "%s " IRC_COLOR_WHITE "| " IRC_COLOR_GREEN "%s kicked %s (%s)", IRC_COLOR_IRC_TO_WSW, chan, nick, victim, trailing);
	}
}

static void Irc_Client_CmdNick_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	if (!strcmp(Cvar_GetStringValue(irc_nick), nick))
		irc_nick = IRC_IMPORT.Cvar_ForceSet("irc_nick", trailing);
	Irc_Println(IRC_COLOR_GREEN "%s is now known as %s", IRC_COLOR_IRC_TO_WSW, nick, trailing);
}

#ifdef _DEBUG
static void Irc_Client_CmdGeneric_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	switch (cmd.type) {
		case IRC_COMMAND_NUMERIC:
			Irc_Println(IRC_COLOR_WHITE "<%s> [%03d] %s : %s", IRC_COLOR_IRC_TO_WSW, prefix, cmd.numeric, params, trailing);
			break;
		case IRC_COMMAND_STRING:
			Irc_Println(IRC_COLOR_WHITE "<%s> [%s] %s : %s", IRC_COLOR_IRC_TO_WSW, prefix, cmd.string, params, trailing);
			break;
	}
}
#endif
