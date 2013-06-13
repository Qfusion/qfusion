#ifndef IRC_PROTOCOL_H
#define IRC_PROTOCOL_H

// connect to or disconnect from server
qboolean Irc_Proto_Connect(const char *host, unsigned short port);
qboolean Irc_Proto_Disconnect(void);

// client -> server messages
qboolean Irc_Proto_Quit(const char *msg);
qboolean Irc_Proto_Nick(const char *nick);
qboolean Irc_Proto_User(const char *user, qboolean invisible, const char *name);
qboolean Irc_Proto_Password(const char *password);
qboolean Irc_Proto_Join(const char *channel, const char *password);
qboolean Irc_Proto_Part(const char *channel);
qboolean Irc_Proto_Mode(const char *target, const char *modes, const char *params);
qboolean Irc_Proto_Topic(const char *channel, const char *topic);
qboolean Irc_Proto_Msg(const char *target, const char *msg);
qboolean Irc_Proto_Notice(const char *target, const char *msg);
qboolean Irc_Proto_Pong(const char *nick, const char *server, const char *cookie);
qboolean Irc_Proto_Kick(const char *channel, const char *nick, const char *reason);
qboolean Irc_Proto_Who(const char *nick);
qboolean Irc_Proto_Whois(const char *nick);
qboolean Irc_Proto_Whowas(const char *nick);
qboolean Irc_Proto_Quote(const char *message);

// flush send buffer according to leaky bucket protocol (flood protection)
qboolean Irc_Proto_Flush(void);

#define IRC_SEND_BUF_SIZE 512
#define IRC_RECV_BUF_SIZE 1024

extern cvar_t *irc_messageBucketSize;
extern cvar_t *irc_messageBucketBurst;
extern cvar_t *irc_messageBucketRate;
extern cvar_t *irc_characterBucketSize;
extern cvar_t *irc_characterBucketBurst;
extern cvar_t *irc_characterBucketRate;

#define IRC_DEFAULT_MESSAGE_BUCKET_SIZE		100		// max 100 messages in bucket
#define IRC_DEFAULT_MESSAGE_BUCKET_BURST	5		// max burst size is 5 messages
#define IRC_DEFAULT_MESSAGE_BUCKET_RATE		0.5		// per second (5 messages in 10 seconds)
#define IRC_DEFAULT_CHARACTER_BUCKET_SIZE	2500	// max 2,500 characters in bucket
#define IRC_DEFAULT_CHARACTER_BUCKET_BURST	250		// max burst size is 250 characters
#define IRC_DEFAULT_CHARACTER_BUCKET_RATE	10		// per second (100 characters in 10 seconds)

// server <- client messages
typedef struct irc_server_msg_s {
	union {
		char string[IRC_SEND_BUF_SIZE];
		irc_numeric_t numeric;
	};
	irc_command_type_t type;
	char prefix[IRC_SEND_BUF_SIZE];
	char params[IRC_SEND_BUF_SIZE];
	char trailing[IRC_SEND_BUF_SIZE];
} irc_server_msg_t;

qboolean Irc_Proto_PollServerMsg(irc_server_msg_t *msg, qboolean *msg_complete);
qboolean Irc_Proto_ProcessServerMsg(const irc_server_msg_t *msg);

static const qboolean IRC_INVISIBLE = qtrue;
static const char IRC_QUIT_MSG[] = APP_URL;

#endif
