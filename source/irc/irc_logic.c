#include "irc_common.h"
#include "irc_logic.h"
#include "irc_protocol.h"
#include "irc_listeners.h"

struct irc_channel_s {
	char *name;
	char *topic;
	trie_t *names;
};

extern dynvar_t *irc_connected;

dynvar_t *irc_channels = NULL;
cvar_t *irc_ctcpReplies = NULL;

static char *defaultChan_str = NULL;

static const quint64 IRC_TRANSMIT_INTERVAL = 10;
static trie_t *chan_trie = NULL;

static void Irc_Logic_Frame_f(void *frame);

static irc_channel_t *Irc_Logic_AddChannel(const char *name);
static void Irc_Logic_RemoveChannel(irc_channel_t *channel);
static void Irc_Logic_SetChannelTopic(irc_channel_t *channel, const char *topic);
static void Irc_Logic_AddChannelName(irc_channel_t *channel, irc_nick_prefix_t prefix, const char *nick);
static void Irc_Logic_RemoveChannelName(irc_channel_t *channel, const char *nick);

static void Irc_Logic_CmdPing_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdError_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdMode_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdJoin_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdPart_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdQuit_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdKill_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdNick_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdKick_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdTopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdPrivmsg_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdRplNamreply_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdRplTopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Logic_CmdRplNotopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);

static void Irc_Logic_SetNick_f(void);

static dynvar_get_status_t Irc_Logic_GetDefaultChan_f(void **chan);
static dynvar_set_status_t Irc_Logic_SetDefaultChan_f(void *chan);
static dynvar_get_status_t Irc_Logic_GetChannels_f(void **channels);

static char *Irc_Logic_DumpChannelNames(void);

void Irc_Logic_Connect(const char *server, unsigned short port) {
	qboolean connected = qfalse;
	if (!Irc_Proto_Connect(server, port)) {
		// connected to server, send NICK and USER commands
		cvar_t * const irc_user = IRC_IMPORT.Cvar_Get("irc_user", APPLICATION "User", CVAR_ARCHIVE);
		cvar_t * const irc_nick = IRC_IMPORT.Cvar_Get("irc_nick", APPLICATION "Player", CVAR_ARCHIVE);
		cvar_t * const irc_password = IRC_IMPORT.Cvar_Get("irc_password", "", CVAR_ARCHIVE);
		const char * const pass = Cvar_GetStringValue(irc_password);
		const char * const user = Cvar_GetStringValue(irc_user);
		if (strlen(pass))
			Irc_Proto_Password(pass);
		Irc_Proto_Nick(Cvar_GetStringValue(irc_nick));
		Irc_Proto_User(user, IRC_INVISIBLE, user);
		connected = !Irc_Proto_Flush();
	}
	if (connected)
		IRC_IMPORT.Dynvar_SetValue(irc_connected, &connected);
}

void Irc_Logic_Disconnect(const char *reason) {
	qboolean *old_c;
	IRC_IMPORT.Dynvar_GetValue(irc_connected, (void**) &old_c);
	if (*old_c) {
		char buf[1024];
		qboolean new_c = qfalse;
		strcpy(IRC_ERROR_MSG, reason);
		Irc_ColorFilter(IRC_QUIT_MSG, IRC_COLOR_WSW_TO_IRC, buf);
		Irc_Proto_Quit(buf);
		Irc_Proto_Disconnect();
		IRC_IMPORT.Dynvar_SetValue(irc_connected, &new_c);
	}
}

void Irc_Logic_Connected_f(void *connected) {
	dynvar_t * const frametick = IRC_IMPORT.Dynvar_Lookup("frametick");
	const qboolean c = * (qboolean*) connected;
	assert(frametick);
	if (c) {
		// connected
		irc_command_t cmd;
		cmd.type = IRC_COMMAND_STRING;
		cmd.string = "PING";				Irc_Proto_AddListener(cmd, Irc_Logic_CmdPing_f);
		cmd.string = "ERROR";				Irc_Proto_AddListener(cmd, Irc_Logic_CmdError_f);
		cmd.string = "MODE";				Irc_Proto_AddListener(cmd, Irc_Logic_CmdMode_f);
		cmd.string = "JOIN";				Irc_Proto_AddListener(cmd, Irc_Logic_CmdJoin_f);
		cmd.string = "PART";				Irc_Proto_AddListener(cmd, Irc_Logic_CmdPart_f);
		cmd.string = "TOPIC";				Irc_Proto_AddListener(cmd, Irc_Logic_CmdTopic_f);
		cmd.string = "NICK";				Irc_Proto_AddListener(cmd, Irc_Logic_CmdNick_f);
		cmd.string = "QUIT";				Irc_Proto_AddListener(cmd, Irc_Logic_CmdQuit_f);
		cmd.string = "KILL";				Irc_Proto_AddListener(cmd, Irc_Logic_CmdKill_f);
		cmd.string = "KICK";				Irc_Proto_AddListener(cmd, Irc_Logic_CmdKick_f);
		cmd.string = "PRIVMSG";				Irc_Proto_AddListener(cmd, Irc_Logic_CmdPrivmsg_f);
		cmd.type = IRC_COMMAND_NUMERIC;
		cmd.numeric = RPL_NAMREPLY;			Irc_Proto_AddListener(cmd, Irc_Logic_CmdRplNamreply_f);
		cmd.numeric = RPL_TOPIC;			Irc_Proto_AddListener(cmd, Irc_Logic_CmdRplTopic_f);
		cmd.numeric = RPL_NOTOPIC;			Irc_Proto_AddListener(cmd, Irc_Logic_CmdRplNotopic_f);
		IRC_IMPORT.Dynvar_AddListener(frametick, Irc_Logic_Frame_f);
		Cvar_FlagSet(&irc_nick->flags, CVAR_READONLY);
		IRC_IMPORT.Cmd_AddCommand("irc_setNick", Irc_Logic_SetNick_f);
		IRC_IMPORT.Cvar_Set(irc_defaultChannel->name, "");
		irc_channels = IRC_IMPORT.Dynvar_Create("irc_channels", qtrue, Irc_Logic_GetChannels_f, IRC_IMPORT.DYNVAR_READONLY);
		irc_ctcpReplies = IRC_IMPORT.Cvar_Get("irc_ctcpReplies", "1", CVAR_ARCHIVE);
		assert(!chan_trie);
		IRC_IMPORT.Trie_Create(TRIE_CASE_SENSITIVE, &chan_trie);
		assert(chan_trie);
	} else {
		// disconnected
		trie_dump_t *chan_dump;
		unsigned int i;
		irc_command_t cmd;
		cmd.type = IRC_COMMAND_STRING;
		cmd.string = "ERROR";				Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdError_f);
		cmd.string = "PING";				Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdPing_f);
		cmd.string = "MODE";				Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdMode_f);
		cmd.string = "JOIN";				Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdJoin_f);
		cmd.string = "PART";				Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdPart_f);
		cmd.string = "TOPIC";				Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdTopic_f);
		cmd.string = "NICK";				Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdNick_f);
		cmd.string = "QUIT";				Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdQuit_f);
		cmd.string = "KILL";				Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdKill_f);
		cmd.string = "KICK";				Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdKick_f);
		cmd.string = "PRIVMSG";				Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdPrivmsg_f);
		cmd.type = IRC_COMMAND_NUMERIC;
		cmd.numeric = RPL_NAMREPLY;			Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdRplNamreply_f);
		cmd.numeric = RPL_TOPIC;			Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdRplTopic_f);
		cmd.numeric = RPL_NOTOPIC;			Irc_Proto_RemoveListener(cmd, Irc_Logic_CmdRplNotopic_f);
		Cvar_FlagUnset(&irc_nick->flags, CVAR_READONLY);
		IRC_IMPORT.Cmd_RemoveCommand("irc_setNick");
		IRC_IMPORT.Dynvar_RemoveListener(frametick, Irc_Logic_Frame_f);
		Irc_MemFree(defaultChan_str);
		IRC_IMPORT.Dynvar_Destroy(irc_channels);
		irc_channels = NULL;
		defaultChan_str = NULL;
		assert(chan_trie);
		IRC_IMPORT.Trie_Dump(chan_trie, "", TRIE_DUMP_VALUES, &chan_dump);
		for (i = 0; i < chan_dump->size; ++i) {
			irc_channel_t * const chan = (irc_channel_t*) chan_dump->key_value_vector[i].value;
			Irc_MemFree(chan->name);
			Irc_MemFree(chan->topic);
			IRC_IMPORT.Trie_Destroy(chan->names);
		}
		IRC_IMPORT.Trie_FreeDump(chan_dump);
		IRC_IMPORT.Trie_Destroy(chan_trie);
		chan_trie = NULL;
	}
}

unsigned int Irc_Logic_NoOfChannels(void) {
	unsigned int size;
	assert(chan_trie);
	IRC_IMPORT.Trie_GetSize(chan_trie, &size);
	return size;
}

static irc_channel_t *Irc_Logic_AddChannel(const char *name) {
	irc_channel_t *chan = Irc_MemAlloc(sizeof(irc_channel_t));
	assert(name);
	assert(chan_trie);
	if (IRC_IMPORT.Trie_Insert(chan_trie, name, chan) == TRIE_OK) {
		chan->name = Irc_MemAlloc(strlen(name) + 1);
		strcpy(chan->name, name);
		IRC_IMPORT.Trie_Create(TRIE_CASE_SENSITIVE, &chan->names);
		chan->topic = Irc_MemAlloc(1);	chan->topic[0] = '\0';
		if (Irc_Logic_NoOfChannels() == 1) {
			IRC_IMPORT.Cvar_Set(irc_defaultChannel->name, name);
		}
		IRC_IMPORT.Dynvar_CallListeners(irc_channels, Irc_Logic_DumpChannelNames());
		return chan;
	} else {
		Irc_MemFree(chan);
		return 0;
	}
}

static void Irc_Logic_RemoveChannel(irc_channel_t *channel) {
	irc_channel_t *chan;
	assert(channel);
	assert(chan_trie);
	if (IRC_IMPORT.Trie_Remove(chan_trie, channel->name, (void**) &chan) == TRIE_OK) {
		const char *oldDefaultChan;
		oldDefaultChan = Cvar_GetStringValue(irc_defaultChannel);
		if (!Irc_Logic_NoOfChannels())
			IRC_IMPORT.Cvar_Set(irc_defaultChannel->name, "");
		else if (!strcmp(channel->name, oldDefaultChan)) {
			char *newDefaultChan;
			trie_dump_t *dump;
			IRC_IMPORT.Trie_Dump(chan_trie, "", TRIE_DUMP_KEYS, &dump);
			assert(dump->size);
			newDefaultChan = (char*) dump->key_value_vector[0].key;
			Irc_Printf("Warning: Left default channel. New default channel is \"%s\".\n", newDefaultChan);
			IRC_IMPORT.Cvar_Set(irc_defaultChannel->name, newDefaultChan);
			IRC_IMPORT.Trie_FreeDump(dump);
		}
		IRC_IMPORT.Trie_Destroy(channel->names);
		Irc_MemFree(channel->name);
		Irc_MemFree(channel->topic);
		Irc_MemFree(channel);
		IRC_IMPORT.Dynvar_CallListeners(irc_channels, Irc_Logic_DumpChannelNames());
	}
}

irc_channel_t *Irc_Logic_GetChannel(const char *name) {
	irc_channel_t *chan;
	IRC_IMPORT.Trie_Find(chan_trie, name, TRIE_EXACT_MATCH, (void**) &chan);
	return chan;
}

irc_channel_t * const *Irc_Logic_DumpChannels(void) {
	trie_dump_t *dump;
	irc_channel_t **result;
	unsigned int i;
	assert(chan_trie);
	IRC_IMPORT.Trie_Dump(chan_trie, "", TRIE_DUMP_VALUES, &dump);
	result = Irc_MemAlloc(sizeof(irc_channel_t*) * (dump->size + 1));
	for (i = 0; i < dump->size; ++i)
		result[i] = (irc_channel_t*) dump->key_value_vector[i].value;
	result[dump->size] = NULL;
	IRC_IMPORT.Trie_FreeDump(dump);
	return result;
}

void Irc_Logic_FreeChannelDump(irc_channel_t * const *dump) {
	Irc_MemFree((irc_channel_t**) dump);
}

const char *Irc_Logic_GetChannelName(const irc_channel_t *channel) {
	assert(channel);
	return channel->name;
}

const char *Irc_Logic_GetChannelTopic(const irc_channel_t *channel) {
	assert(channel);
	return channel->topic;
}

static void Irc_Logic_SetChannelTopic(irc_channel_t *channel, const char *topic) {
	assert(channel);
	assert(topic);
	Irc_MemFree(channel->topic);
	channel->topic = Irc_MemAlloc(strlen(topic) + 1);
	strcpy(channel->topic, topic);
}

static void Irc_Logic_AddChannelName(irc_channel_t *channel, irc_nick_prefix_t prefix, const char *nick) {
	IRC_IMPORT.Trie_Insert(channel->names, nick, Irc_GetStaticPrefix(prefix));
}

static void Irc_Logic_RemoveChannelName(irc_channel_t *channel, const char *nick) {
	irc_nick_prefix_t *pprefix;
	IRC_IMPORT.Trie_Remove(channel->names, nick, (void**) &pprefix);
}

const trie_t *Irc_Logic_GetChannelNames(const irc_channel_t *channel) {
	return channel->names;
}

static void Irc_Logic_SendMessages(void) {
	if (Irc_Proto_Flush()) {
		// flush failed, server closed connection
		qboolean connected = qfalse;
		IRC_IMPORT.Dynvar_SetValue(irc_connected, (void*) &connected);
	}
}

static void Irc_Logic_ReadMessages(void) {
	qboolean msg_complete;
	qboolean *connected;
	do {
		irc_server_msg_t msg;
		if (!Irc_Proto_PollServerMsg(&msg, &msg_complete)) {
			// success
			if (msg_complete)
				Irc_Proto_ProcessServerMsg(&msg);
		} else
			// failure
			Irc_Logic_Disconnect("Server closed connection");
		// we need to check connection status since server msg might have caused a
		// disconnect in an message handler function
		IRC_IMPORT.Dynvar_GetValue(irc_connected, (void**) &connected);
	} while (msg_complete && *connected);
}

static void Irc_Logic_Frame_f(void *frame) {
	const quint64 f = * (quint64*) frame;
	if (!(f % IRC_TRANSMIT_INTERVAL)) {
		Irc_Logic_SendMessages();
		Irc_Logic_ReadMessages();
	}
}

static void Irc_Logic_CmdPing_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	Irc_Proto_Pong(Cvar_GetStringValue(irc_nick), params, trailing[0] ? trailing : NULL);
}

static void Irc_Logic_CmdError_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	Irc_Logic_Disconnect(trailing);
}

static void Irc_Logic_CmdMode_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {

	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t pfx;
	char buf[IRC_SEND_BUF_SIZE];
	char *p;
	unsigned int i = 0, j = 0;
	const char *target;
	irc_mode_elem_t modes[IRC_SEND_BUF_SIZE];
	unsigned int no_of_modes = 0;
	irc_channel_t *channel = NULL;

	Irc_ParseName(prefix, nick, &pfx);

	// parse params
	strcpy(buf, params);
	for (p = strtok(buf, " "); p; p = strtok(NULL, " "), ++i) {
		if (i == 0) {
			// mode target (channel or user)
			target = p;
			channel = Irc_Logic_GetChannel(target);
		} else if (channel && i == 1) {
			// mode mask
			qboolean plus_toggle = qtrue;
			const char *e;
			for (e = p; *e; ++e) {
				switch (*e) {
					case '+':
						plus_toggle = qtrue;
						break;
					case '-':
						plus_toggle = qfalse;
						break;
					default:
						modes[no_of_modes].plus = plus_toggle;
						modes[no_of_modes].flag = *e;
						++no_of_modes;
						break;
				}
			}
		} else if (channel) {
			// parse flag parameter
			while (j < no_of_modes && modes[j].flag != IRC_MODE_OP && modes[j].flag != IRC_MODE_VOICE && modes[j].flag != IRC_MODE_BAN && modes[j].flag != IRC_MODE_LIMIT && modes[j].flag != IRC_MODE_KEY)
				++j;	// skip parameterless modes (we don't use them anyway)
			if (j < no_of_modes) {
				switch(modes[j].flag) {
					case IRC_MODE_OP:
					case IRC_MODE_VOICE:
						{
							// expect nick or hostmask
							char nick[256];
							irc_nick_prefix_t ignored_prefix;
							irc_nick_prefix_t *old_prefix;
							Irc_ParseName(p, nick, &ignored_prefix);
							if (IRC_IMPORT.Trie_Find(channel->names, nick, TRIE_EXACT_MATCH, (void**) &old_prefix) == TRIE_OK) {
								if (modes[j].plus) {
									// add prefix
									switch(modes[j].flag) {
										case IRC_MODE_OP:
											// make op
											if (*old_prefix != IRC_NICK_PREFIX_OP)
												IRC_IMPORT.Trie_Replace(channel->names, nick, (void*) Irc_GetStaticPrefix(IRC_NICK_PREFIX_OP), (void**) &old_prefix);
											break;
										case IRC_MODE_VOICE:
											// give voice
											if (*old_prefix == IRC_NICK_PREFIX_NONE)
												IRC_IMPORT.Trie_Replace(channel->names, nick, (void*) Irc_GetStaticPrefix(IRC_NICK_PREFIX_VOICE), (void**) &old_prefix);
											break;
										default:
											// ignored
											break;
									}
								} else {
									// remove prefix
									switch(modes[j].flag) {
										case IRC_MODE_OP:
											// remove op
											if (*old_prefix == IRC_NICK_PREFIX_OP)
												IRC_IMPORT.Trie_Replace(channel->names, nick, (void*) Irc_GetStaticPrefix(IRC_NICK_PREFIX_NONE), (void**) &old_prefix);
											break;
										case IRC_MODE_VOICE:
											// remove voice
											if (*old_prefix == IRC_NICK_PREFIX_VOICE)
												IRC_IMPORT.Trie_Replace(channel->names, nick, (void*) Irc_GetStaticPrefix(IRC_NICK_PREFIX_NONE), (void**) &old_prefix);
											break;
										default:
											// ignored
											break;
									}
								}
							}
						}
						break;
					case IRC_MODE_BAN:
						// expect hostmask
						// not implemented yet (should we care?)
						break;
					case IRC_MODE_LIMIT:
					case IRC_MODE_KEY:	
						// not implemented yet (should we care?)
						break;
					default:
						// would be a programming error
						assert(0);
						break;
				}
				++j;
			}
		}
	}
}

static void Irc_Logic_CmdJoin_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *chan = params[0]
		? params
		: trailing;
	irc_channel_t *channel = Irc_Logic_GetChannel(chan);
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	Irc_ParseName(prefix, nick, &p);
	if (!strcmp(Cvar_GetStringValue(irc_nick), nick) && !channel) {
		// we just joined a channel
		assert(!channel);
		channel = Irc_Logic_AddChannel(chan);
	}
	if (channel) {
		// we are on that channel
		Irc_Logic_AddChannelName(channel, IRC_NICK_PREFIX_NONE, nick);
	}
}

static void Irc_Logic_CmdPart_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	irc_channel_t *channel = Irc_Logic_GetChannel(params);
	if (channel) {
		char nick[IRC_SEND_BUF_SIZE];
		irc_nick_prefix_t p;
		Irc_ParseName(prefix, nick, &p);
		if (!strcmp(nick, Cvar_GetStringValue(irc_nick))) {
			// it's us!
			Irc_Logic_RemoveChannel(channel);
		} else {
			// it's someone else
			Irc_Logic_RemoveChannelName(channel, nick);
		}
	}
}

static void Irc_Logic_CmdQuit_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	irc_channel_t * const *channels, * const *c;
	Irc_ParseName(prefix, nick, &p);
	channels = Irc_Logic_DumpChannels();
	for (c = channels; *c; ++c)
		Irc_Logic_RemoveChannelName(*c, nick);
	Irc_Logic_FreeChannelDump(channels);
}

static void Irc_Logic_CmdKill_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	Irc_Logic_CmdQuit_f(cmd, prefix, params, trailing);
}

static void Irc_Logic_CmdKick_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char buf[IRC_SEND_BUF_SIZE];
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p;
	const char *chan, *victim;
	irc_channel_t *channel;
	Irc_ParseName(prefix, nick, &p);
	strcpy(buf, params);
	chan = strtok(buf, " ");
	victim = strtok(NULL, " ");
	channel = Irc_Logic_GetChannel(chan);
	if (channel) {
		// we are actually on that channel
		if (!strcmp(victim, Cvar_GetStringValue(irc_nick))) {
			// we have been kicked
			Irc_Logic_RemoveChannel(channel);
		} else {
			// someone else was kicked
			Irc_Logic_RemoveChannelName(channel, victim);
		}
	}
}

static void Irc_Logic_CmdNick_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t p, *pp;
	irc_channel_t * const *channels, * const *c;
	Irc_ParseName(prefix, nick, &p);
	if (!strcmp(Cvar_GetStringValue(irc_nick), nick))
		irc_nick = IRC_IMPORT.Cvar_ForceSet("irc_nick", trailing);	// we changed nick
	channels = Irc_Logic_DumpChannels();
	for (c = channels; *c; ++c) {
		// replace name in channel state
		if (IRC_IMPORT.Trie_Find((*c)->names, nick, TRIE_EXACT_MATCH, (void**) &pp) == TRIE_OK) {
			// user was on that channel
			assert(pp);
			p = *pp;
			Irc_Logic_RemoveChannelName(*c, nick);
			Irc_Logic_AddChannelName(*c, p, trailing);
		}
	}
	Irc_Logic_FreeChannelDump(channels);
}

static void Irc_Logic_CmdTopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	irc_channel_t * const channel = Irc_Logic_GetChannel(params);
	if (channel) {
		// we are on that channel
		char nick[IRC_SEND_BUF_SIZE];
		char channel_topic_str[IRC_SEND_BUF_SIZE];
		irc_nick_prefix_t p;
		Irc_ParseName(prefix, nick, &p);
		Irc_ColorFilter(trailing, IRC_COLOR_IRC_TO_WSW, channel_topic_str);
		Irc_Logic_SetChannelTopic(channel, channel_topic_str);
	}
}

static void Irc_Logic_CmdPrivmsg_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	if (Cvar_GetIntegerValue(irc_ctcpReplies)) {
		char nick[IRC_SEND_BUF_SIZE];
		char * const emph = strchr(prefix, '!');
		memset(nick, 0, sizeof(nick));
		if (emph)
			memcpy(nick, prefix, emph - prefix);
		else
			strcpy(nick, prefix);
		if (*params != '#' && *params != '&') {
			// is private message
			if (*trailing == IRC_CTCP_MARKER_CHR) {
				// is probably a CTCP message
				if (!strcmp(trailing + 1, "FINGER" IRC_CTCP_MARKER_STR))
					// Returns the user's full name, and idle time.
					; // NOT SUPPORTED
				else if (!strcmp(trailing + 1, "VERSION" IRC_CTCP_MARKER_STR))
					// The version and type of the client.
					Irc_Proto_Notice(nick, IRC_CTCP_MARKER_STR "VERSION " APPLICATION "IRC " IRC_LOGIC_VERSION " " BUILDSTRING " " ARCH IRC_CTCP_MARKER_STR);
				else if (!strcmp(trailing + 1, "SOURCE" IRC_CTCP_MARKER_STR))
					// Where to obtain a copy of a client.
					; // NOT SUPPORTED
				else if (!strcmp(trailing + 1, "USERINFO" IRC_CTCP_MARKER_STR))
					// A string set by the user (never the client coder)
					; // NOT SUPPORTED
				else if (!strcmp(trailing + 1, "CLIENTINFO" IRC_CTCP_MARKER_STR))
					// Dynamic master index of what a client knows.
					; // NOT SUPPORTED
				else if (!strcmp(trailing + 1, "ERRMSG" IRC_CTCP_MARKER_STR))
					// Used when an error needs to be replied with.
					; // NOT SUPPORTED
				else if (!strncmp(trailing + 1, "PING", 4)) {
					// Used to measure the delay of the IRC network between clients.
					char response[IRC_SEND_BUF_SIZE];
					strcpy(response, trailing);
					response[2] = 'O';					// PING -> PONG
					Irc_Proto_Notice(nick, response);
				} else if (!strcmp(trailing + 1, "TIME" IRC_CTCP_MARKER_STR)) {
					// Gets the local date and time from other clients.
					const time_t t = time(NULL);
					char response[IRC_SEND_BUF_SIZE];
					const size_t response_len = sprintf(response, IRC_CTCP_MARKER_STR "TIME :%s" IRC_CTCP_MARKER_STR, ctime(&t));
					response[response_len - 1] = '\0';	// remove trailing \n
					Irc_Proto_Notice(nick, response);
				}
			}
		}
	}
}

static void Irc_Logic_CmdRplNamreply_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *chan = strchr(params, ' ');	// skip nick
	if (chan) {
		chan = strchr(chan + 1, ' ');			// skip channel type
		if (chan) {
			irc_channel_t * const channel = Irc_Logic_GetChannel(chan + 1);
			if (channel) {
				char buf[IRC_SEND_BUF_SIZE];
				char *name;
				strcpy(buf, trailing);
				for (name = strtok(buf, " "); name; name = strtok(NULL, " ")) {
					char nick[IRC_SEND_BUF_SIZE];
					irc_nick_prefix_t p;
					Irc_ParseName(name, nick, &p);
					Irc_Logic_AddChannelName(channel, p, nick);
				}
			}
		}
	}
}

static void Irc_Logic_CmdRplTopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	const char *chan = strchr(params, ' ');
	if (chan) {
		irc_channel_t * const channel = Irc_Logic_GetChannel(chan + 1);
		if (channel)
			Irc_Logic_SetChannelTopic(channel, trailing);
	}
}

static void Irc_Logic_CmdRplNotopic_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {	
	const char *chan = strchr(params, ' ');
	if (chan) {
		irc_channel_t * const channel = Irc_Logic_GetChannel(chan + 1);
		if (channel)
			Irc_Logic_SetChannelTopic(channel, "");
	}
}

static void Irc_Logic_SetNick_f(void) {
	const int argc = IRC_IMPORT.Cmd_Argc();
	if (argc == 2) {
		Irc_Proto_Nick(IRC_IMPORT.Cmd_Argv(1));
	} else
		Irc_Printf("usage: irc_setNick <nick>\n");
}

static dynvar_get_status_t Irc_Logic_GetChannels_f(void **channels) {
	*channels = Irc_Logic_DumpChannelNames();
	return DYNVAR_GET_OK;
}

static char *Irc_Logic_DumpChannelNames(void) {
	static char channel_names[1024];
	irc_channel_t * const * const channels = Irc_Logic_DumpChannels();
	irc_channel_t * const *chan;
	char *out = channel_names;
	for (chan = channels; *chan; ++chan) {
		const char *in;
		for (in = Irc_Logic_GetChannelName(*chan); *in; ++in)
			*out++ = *in;
		if (*(chan + 1))
			*out++ = ' ';
	}
	*out = '\0';
	Irc_Logic_FreeChannelDump(channels);
	return channel_names;
}
