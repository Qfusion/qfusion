#include "irc_common.h"
#include "irc_rcon.h"
#include "irc_listeners.h"
#include "irc_protocol.h"

#ifdef _WIN32
#	define strcasecmp stricmp
#endif

typedef struct irc_rcon_user_s {
	unsigned int millis;
} irc_rcon_user_t;

cvar_t *irc_rcon = NULL;
cvar_t *irc_rconTimeout = NULL;

static void Irc_Rcon_CmdPrivmsg_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Rcon_CmdQuit_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);
static void Irc_Rcon_ProcessMsg(const char *user, const char *msg);
static void Irc_Rcon_Flush_f(int redirected, const char *msg, const void *extra);

static trie_t *irc_rcon_users = NULL;
static const char *rcon_flush_to = NULL;

void Irc_Rcon_Connected_f(void *connected) {
	qboolean * const c = (qboolean*) connected;
	if (!irc_rcon)
		irc_rcon = IRC_IMPORT.Cvar_Get("irc_rcon", "0", CVAR_ARCHIVE);
	if (!irc_rconTimeout)
		irc_rconTimeout = IRC_IMPORT.Cvar_Get("irc_rconTimeout", "300", CVAR_ARCHIVE);
	if (*c) {
		irc_command_t cmd;
		cmd.type = IRC_COMMAND_STRING;
		cmd.string = "PRIVMSG";				Irc_Proto_AddListener(cmd, Irc_Rcon_CmdPrivmsg_f);
		cmd.string = "QUIT";				Irc_Proto_AddListener(cmd, Irc_Rcon_CmdQuit_f);
		assert(!irc_rcon_users);
		IRC_IMPORT.Trie_Create(TRIE_CASE_SENSITIVE, &irc_rcon_users);
		assert(irc_rcon_users);
	} else {
		unsigned int i;
		trie_dump_t *dump;
		irc_command_t cmd;
		cmd.type = IRC_COMMAND_STRING;
		cmd.string = "PRIVMSG";				Irc_Proto_RemoveListener(cmd, Irc_Rcon_CmdPrivmsg_f);
		cmd.string = "QUIT";				Irc_Proto_RemoveListener(cmd, Irc_Rcon_CmdQuit_f);
		assert(irc_rcon_users);
		IRC_IMPORT.Trie_Dump(irc_rcon_users, "", TRIE_DUMP_VALUES, &dump);
		for (i = 0; i < dump->size; ++i)
			Irc_MemFree(dump->key_value_vector[i].value);
		IRC_IMPORT.Trie_FreeDump(dump);
		IRC_IMPORT.Trie_Destroy(irc_rcon_users);
		irc_rcon_users = NULL;
	}
}

static void Irc_Rcon_CmdPrivmsg_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	assert(irc_rcon);
	if (Cvar_GetIntegerValue(irc_rcon)) {
		// irc_rcon is set, check for rcon command
		if (*params != '#' && *params != '&') {
			// not a channel message, but a client-to-client message
			Irc_Rcon_ProcessMsg(prefix, trailing);
		}
	}
}

static void Irc_Rcon_CmdQuit_f(irc_command_t cmd, const char *prefix, const char *params, const char *trailing) {
	assert(irc_rcon);
	if (Cvar_GetIntegerValue(irc_rcon)) {
		irc_rcon_user_t *rcon_user;
		if (IRC_IMPORT.Trie_Remove(irc_rcon_users, prefix, (void**) &rcon_user) == TRIE_OK)
			Irc_MemFree(rcon_user);
	}
}

#ifdef _WIN32
#	pragma warning (push)
#	pragma warning (disable : 4996)		// stricmp is deprecated
#endif

static void Irc_Rcon_ProcessMsg(const char *user, const char *msg) {

	static char nick[IRC_SEND_BUF_SIZE];
	irc_nick_prefix_t prefix;
	char *buf = (char*) Irc_MemAlloc((int) strlen(msg) + 1);
	const char *word;

	Irc_ParseName(user, nick, &prefix);
	strcpy(buf, msg);
	word = strtok(buf, " ");
	if (word && !strcasecmp(word, IRC_RCON_PREFIX)) {
		// it really is an RCON message, not a normal PRIVMSG
		unsigned int millis = IRC_IMPORT.Milliseconds();
		irc_rcon_user_t *rcon_user;
		if (IRC_IMPORT.Trie_Find(irc_rcon_users, user, TRIE_EXACT_MATCH, (void**) &rcon_user) == TRIE_OK) {
			// user is already authorized
			const unsigned int timeout = Cvar_GetIntegerValue(irc_rconTimeout);
			if (!timeout || ((millis - rcon_user->millis) / 1000) < timeout) {
				// no timeout, reset user timestamp
				irc_rcon_user_t *rcon_user_old;
				rcon_user->millis = millis;
				IRC_IMPORT.Trie_Replace(irc_rcon_users, user, (void*) rcon_user, (void**) &rcon_user_old);
				assert(rcon_user == rcon_user_old);
				word = strtok(NULL, " ");
				if (word) {
					if (!strcasecmp(word, IRC_RCON_LOGOUT)) {
						// user wants to log off
						Irc_Proto_Msg(nick, "Logged out. You may login again via " IRC_RCON_PREFIX " " IRC_RCON_LOGIN " <rcon_password>.");
						IRC_IMPORT.Trie_Remove(irc_rcon_users, user, (void**) &rcon_user);
						Irc_MemFree(rcon_user);
					} else {
						// redirect console and execute
						char cmd_buf[IRC_SEND_BUF_SIZE + 2];
						char rcon_buf[16384];	// make it big, we don't trust console redirect
						char *c = cmd_buf;
						size_t word_len = strlen(word);
						memset(rcon_buf, 0, sizeof(rcon_buf));
						memcpy(c, word, word_len);
						c += word_len;
						for (word = strtok(NULL, " "); word; word = strtok(NULL, " ")) {
							*c++ = ' ';
							word_len = strlen(word);
							memcpy(c, word, word_len);
							c += word_len;
						}
						*c = '\0';
						rcon_flush_to = nick;
						IRC_IMPORT.Com_BeginRedirect(1, rcon_buf, sizeof(rcon_buf) - 1, Irc_Rcon_Flush_f, NULL);
						IRC_IMPORT.Cmd_ExecuteString(cmd_buf);
						IRC_IMPORT.Com_EndRedirect();
					}
				}
			} else {
				// timeout, inform user
				Irc_Proto_Msg(nick, "Timed out. Please login via " IRC_RCON_PREFIX " " IRC_RCON_LOGIN " <rcon_password>.");
				IRC_IMPORT.Trie_Remove(irc_rcon_users, user, (void**) &rcon_user);
				Irc_MemFree(rcon_user);
			}
		} else {
			// user not authorized, check for IRC_RCON_LOGIN command
			word = strtok(NULL, " ");
			if (word && !strcasecmp(word, IRC_RCON_LOGIN)) {
				const cvar_t * const rcon_password = IRC_IMPORT.Cvar_Get("rcon_password", "", CVAR_ARCHIVE);
				word = strtok(NULL, " ");
				if (word && !strcmp(word, Cvar_GetStringValue(rcon_password))) {
					// password correct, authorize
					Irc_Proto_Msg(nick, "Logged in. You may now issue commands via " IRC_RCON_PREFIX " <command> {<arg>}. Log out via " IRC_RCON_PREFIX " " IRC_RCON_LOGOUT ".");
					rcon_user = (irc_rcon_user_t*) Irc_MemAlloc(sizeof(irc_rcon_user_t));
					rcon_user->millis = millis;
					IRC_IMPORT.Trie_Insert(irc_rcon_users, user, (void*) rcon_user);
				}
			}
		}
	}
	Irc_MemFree(buf);
}

#ifdef _WIN32
#	pragma warning (pop)
#endif

static void Irc_Rcon_Flush_f(int redirected, const char *msg, const void *extra) {
	if (redirected == 1) {
		// cut into lines
		size_t len = strlen(msg);
		char * const outputbuf = (char*) Irc_MemAlloc(len + 1);
		char *line;
		memcpy(outputbuf, msg, len);
		outputbuf[len] = '\0';
		for (line = strtok(outputbuf, "\n"); line; line = strtok(NULL, "\n")) {
			// perform color code translation
			char * const colored_line = (char*) Irc_MemAlloc(strlen(line) * 2);
			char *c = colored_line;
			char chunk[101];
			Irc_ColorFilter(line, IRC_COLOR_WSW_TO_IRC, colored_line);
			// cut line into neat little chunks so the IRC server accepts them
			len = strlen(c);
			while (len) {
				size_t to_copy = min(sizeof(chunk) - 1, len);
				memcpy(chunk, c, to_copy);
				chunk[to_copy] = '\0';
				Irc_Proto_Msg(rcon_flush_to, chunk);
				c += to_copy;
				len -= to_copy;
			}
			Irc_MemFree(colored_line);
		}
		Irc_MemFree(outputbuf);
	}
}
