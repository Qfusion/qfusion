#ifndef IRC_LISTENERS_H
#define IRC_LISTENERS_H

typedef enum irc_mode_flag_e {
	IRC_MODE_OP = 'o',
	IRC_MODE_VOICE = 'v',
	IRC_MODE_BAN = 'b',
	IRC_MODE_PRIVATE = 'p',
	IRC_MODE_SECRET = 's',
	IRC_MODE_INVITEONLY = 'i',
	IRC_MODE_TOPIC = 't',
	IRC_MODE_NOMESS = 'n',
	IRC_MODE_MODERATED = 'm',
	IRC_MODE_LIMIT = 'l',
	IRC_MODE_KEY = 'k'
} irc_mode_flag_t;

typedef struct irc_mode_elem_s {
	qboolean plus;
	irc_mode_flag_t flag;
} irc_mode_elem_t;

void Irc_Proto_InitListeners(void);		// call before doing anything else
void Irc_Proto_TeardownListeners(void);	// call at library shutdown

// listener will be called if matching cmd is received from server
//   multiple listeners can listen to a single command
//   a single listener can listen to multiple commands
void Irc_Proto_AddListener(irc_command_t cmd, irc_listener_f listener);

// listener will not be called anymore when cmd is received from server
void Irc_Proto_RemoveListener(irc_command_t cmd, irc_listener_f listener);

// listener will be called if no specific listener has been found
void Irc_Proto_AddGenericListener(irc_listener_f listener);

// listener will not be called anymore
void Irc_Proto_RemoveGenericListener(irc_listener_f listener);

// call registered listeners
void Irc_Proto_CallListeners(irc_command_t cmd, const char *prefix, const char *params, const char *trailing);

#endif
