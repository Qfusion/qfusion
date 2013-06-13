#include "irc_common.h"
#include "irc_protocol.h"
#include "irc_listeners.h"
#include "irc_net.h"

#include <stdio.h>

#define STRINGIFY(x) #x
#define DOUBLEQUOTE(x) STRINGIFY(x)

cvar_t *irc_messageBucketSize = NULL;
cvar_t *irc_messageBucketBurst = NULL;
cvar_t *irc_messageBucketRate = NULL;
cvar_t *irc_characterBucketSize = NULL;
cvar_t *irc_characterBucketBurst = NULL;
cvar_t *irc_characterBucketRate = NULL;

typedef struct irc_bucket_message_s {
	char *msg;
	size_t msg_len;
	struct irc_bucket_message_s *next;
} irc_bucket_message_t;

typedef struct irc_bucket_s {
	irc_bucket_message_t *first_msg;	// pointer to first message in queue
	unsigned int message_size;			// number of messages in bucket
	unsigned int character_size;		// number of characters in bucket
	quint64 last_refill;				// last refill timestamp
	double message_token;
	double character_token;
} irc_bucket_t;

static qboolean Irc_Proto_ParseServerMsg(const char *txt, size_t txt_len, irc_server_msg_t *msg);

static qboolean Irc_Proto_Enqueue(const char *msg, size_t msg_len);
static void Irc_Proto_RefillBucket(void);
static qboolean Irc_Proto_DrainBucket(void);

static irc_bucket_t irc_bucket;
static irc_socket_t irc_sock;

qboolean Irc_Proto_Connect(const char *host, unsigned short port) {
	const qboolean status = Irc_Net_Connect(host, port, &irc_sock);
	if (!status) {
		if (!irc_messageBucketSize) {
			irc_messageBucketSize = IRC_IMPORT.Cvar_Get("irc_messageBucketSize", DOUBLEQUOTE(IRC_DEFAULT_MESSAGE_BUCKET_SIZE), CVAR_ARCHIVE);
			irc_messageBucketBurst = IRC_IMPORT.Cvar_Get("irc_messageBucketBurst", DOUBLEQUOTE(IRC_DEFAULT_MESSAGE_BUCKET_BURST), CVAR_ARCHIVE);
			irc_messageBucketRate = IRC_IMPORT.Cvar_Get("irc_messageBucketRate", DOUBLEQUOTE(IRC_DEFAULT_MESSAGE_BUCKET_RATE), CVAR_ARCHIVE);
			irc_characterBucketSize = IRC_IMPORT.Cvar_Get("irc_characterBucketSize", DOUBLEQUOTE(IRC_DEFAULT_CHARACTER_BUCKET_SIZE), CVAR_ARCHIVE);
			irc_characterBucketBurst = IRC_IMPORT.Cvar_Get("irc_characterBucketBurst", DOUBLEQUOTE(IRC_DEFAULT_CHARACTER_BUCKET_BURST), CVAR_ARCHIVE);
			irc_characterBucketRate = IRC_IMPORT.Cvar_Get("irc_characterBucketRate", DOUBLEQUOTE(IRC_DEFAULT_CHARACTER_BUCKET_RATE), CVAR_ARCHIVE);
		}
		irc_bucket.first_msg = NULL;
		irc_bucket.message_size = 0;
		irc_bucket.character_size = 0;
		irc_bucket.last_refill = IRC_IMPORT.Microseconds();
		irc_bucket.message_token = Cvar_GetFloatValue(irc_messageBucketBurst);
		irc_bucket.character_token = Cvar_GetFloatValue(irc_characterBucketBurst);
	}
	return status;
}

qboolean Irc_Proto_Disconnect(void) {
	const qboolean status = Irc_Net_Disconnect(irc_sock);
	if (!status) {
		irc_bucket_message_t *msg = irc_bucket.first_msg;
		irc_bucket_message_t *prev;
		while (msg) {
			prev = msg;
			msg = msg->next;
			Irc_MemFree(prev->msg);
			Irc_MemFree(prev);
		}
		irc_bucket.first_msg = NULL;
		irc_bucket.message_size = 0;
		irc_bucket.character_size = 0;
	}
	return status;
}

qboolean Irc_Proto_Quit(const char *quitmsg) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "QUIT %s\r\n", quitmsg);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Net_Send(irc_sock, msg, msg_len);	// send immediately
}

qboolean Irc_Proto_Nick(const char *nick) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "NICK %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_User(const char *user, qboolean invisible, const char *name) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "USER %s %c * :%s\r\n", user, invisible ? '8' : '0', name);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Password(const char *password) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "PASS %s\r\n", password);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Join(const char *channel, const char *password) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = password
		? snprintf(msg, sizeof(msg) - 1, "JOIN %s %s\r\n", channel, password)
		: snprintf(msg, sizeof(msg) - 1, "JOIN %s\r\n", channel);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Part(const char *channel) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "PART %s\r\n", channel);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Mode(const char *target, const char *modes, const char *params) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = params
		? snprintf(msg, sizeof(msg) - 1, "MODE %s %s %s\r\n", target, modes, params)
		: snprintf(msg, sizeof(msg) - 1, "MODE %s %s\r\n", target, modes);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Topic(const char *channel, const char *topic) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = topic
		? snprintf(msg, sizeof(msg) - 1, "TOPIC %s :%s\r\n", channel, topic)
		: snprintf(msg, sizeof(msg) - 1, "TOPIC %s\r\n", channel);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Msg(const char *target, const char *text) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "PRIVMSG %s :%s\r\n", target, text);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Notice(const char *target, const char *text) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "NOTICE %s :%s\r\n", target, text);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Pong(const char *nick, const char *server, const char *cookie) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = cookie
		? snprintf(msg, sizeof(msg) - 1, "PONG %s %s :%s\r\n", nick, server, cookie)
		: snprintf(msg, sizeof(msg) - 1, "PONG %s %s\r\n", nick, server);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Net_Send(irc_sock, msg, msg_len);	// send immediately
}

qboolean Irc_Proto_Kick(const char *channel, const char *nick, const char *reason) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = reason
		? snprintf(msg, sizeof(msg) - 1, "KICK %s %s :%s\r\n", channel, nick, reason)
		: snprintf(msg, sizeof(msg) - 1, "KICK %s %s :%s\r\n", channel, nick, nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Who(const char *nick) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "WHO %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Whois(const char *nick) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "WHOIS %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Whowas(const char *nick) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "WHOWAS %s\r\n", nick);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_Quote(const char *message) {
	char msg[IRC_SEND_BUF_SIZE];
	const int msg_len = snprintf(msg, sizeof(msg) - 1, "%s\r\n", message);
	msg[sizeof(msg) - 1] = '\0';
	return Irc_Proto_Enqueue(msg, msg_len);
}

qboolean Irc_Proto_PollServerMsg(irc_server_msg_t *msg, qboolean *msg_complete) {
	static char buf[IRC_RECV_BUF_SIZE];
	static char *last = buf;
	int recvd;
	*msg_complete = qfalse;
	// recv packet
	if (Irc_Net_Receive(irc_sock, last, sizeof(buf) - (last - buf) - 1, &recvd)) {
		// receive failed
		return qtrue;
	} else {
		// terminate buf string
		const char * const begin = buf;
		last += recvd;
		*last = '\0';
		if (last != begin) {
			// buffer not empty;
			const char * const end = strstr(begin, "\r\n");
			if (end) {
				// complete command in buffer, parse
				const size_t cmd_len = end + 2 - begin;
				if (!Irc_Proto_ParseServerMsg(begin, cmd_len, msg)) {
					// parsing successful
					// move succeeding commands to begin of buffer
					memmove(buf, end + 2, sizeof(buf) - cmd_len);
					last -= cmd_len;
					*msg_complete = qtrue;
				} else {
					// parsing failure, fatal
					strcpy(IRC_ERROR_MSG, "Received invalid packet from server");
					return qtrue;
				}
			}
		} else
			*msg_complete = qfalse;
		return qfalse;
	}
}

qboolean Irc_Proto_ProcessServerMsg(const irc_server_msg_t *msg) {
	irc_command_t cmd;
	cmd.type = msg->type;
	switch (cmd.type) {
		case IRC_COMMAND_NUMERIC:
			cmd.numeric = msg->numeric;
			break;
		case IRC_COMMAND_STRING:
			cmd.string = msg->string;
			break;
	}
	Irc_Proto_CallListeners(cmd, msg->prefix, msg->params, msg->trailing);
	return qfalse;
}

static qboolean Irc_Proto_ParseServerMsg(const char *txt, size_t txt_len, irc_server_msg_t *msg) {
	const char *c = txt;
	const char *end = txt + txt_len;
	*(msg->prefix) = '\0';
	*(msg->params) = '\0';
	*(msg->trailing) = '\0';
	if (c < end && *c == ':') {
		// parse prefix
		char *prefix = msg->prefix;
		++c;
		while (c < end && *c != '\r' && *c != ' ') {
			*prefix = *c;
			++prefix;
			++c;
		}
		*prefix = '\0';
		++c;
	}
	if (c < end && *c != '\r') {
		// parse command
		if (c < end && *c >= '0' && *c <= '9') {
			// numeric command
			char command[4];
			int i;
			for (i = 0; i < 3; ++i) {
				if (c < end && *c >= '0' && *c <= '9') {
					command[i] = *c;
					++c;
				} else
					return qtrue;
			}
			command[3] = '\0';
			msg->type = IRC_COMMAND_NUMERIC;
			msg->numeric = atoi(command);
		} else if (c < end && *c != '\r') {
			// string command
			char *command = msg->string;
			while (c < end && *c != '\r' && *c != ' ') {
				*command = *c;
				++command;
				++c;
			}
			*command = '\0';
			msg->type = IRC_COMMAND_STRING;
		} else
			return qtrue;
		if (c < end && *c == ' ') {
			// parse params and trailing
			char *params = msg->params;
			++c;
			while (c < end && *c != '\r' && *c != ':') {
				// parse params
				while (c < end && *c != '\r' && *c != ' ') {
					// parse single param
					*params = *c;
					++params;
					++c;
				}
				if (c + 1 < end && *c == ' ' && *(c+1) != ':') {
					// more params
					*params = ' ';
					++params;
				}
				if (*c == ' ')
					++c;
			}
			*params = '\0';
			if (c < end && *c == ':') {
				// parse trailing
				char *trailing = msg->trailing;
				++c;
				while (c < end && *c != '\r') {
					*trailing = *c;
					++trailing;
					++c;
				}
				*trailing = '\0';
			}
		}
	}
	return qfalse;
}

qboolean Irc_Proto_Flush(void) {
	Irc_Proto_RefillBucket();		// first refill token
	return Irc_Proto_DrainBucket();	// then send messages (if allowed)
}

static qboolean Irc_Proto_Enqueue(const char *msg, size_t msg_len) {
	// create message node
	const double messageBucketSize = Cvar_GetFloatValue(irc_messageBucketSize);
	const double characterBucketSize = Cvar_GetFloatValue(irc_characterBucketSize);
	irc_bucket_message_t * const m = (irc_bucket_message_t*) Irc_MemAlloc(sizeof(irc_bucket_message_t));
	irc_bucket_message_t * n = irc_bucket.first_msg;
	if (irc_bucket.message_size + 1 <= messageBucketSize && irc_bucket.character_size + msg_len <= characterBucketSize) {
		m->msg = (char*) Irc_MemAlloc(msg_len);
		memcpy(m->msg, msg, msg_len);
		m->msg_len = msg_len;
		m->next = NULL;
		// append message node
		if (n) {
			while (n->next)
				n = n->next;
			n->next = m;
		} else
			irc_bucket.first_msg = m;
		// update bucket sizes
		++irc_bucket.message_size;
		irc_bucket.character_size += msg_len;
		return qfalse;
	} else {
		strcpy(IRC_ERROR_MSG, "Bucket(s) full. Could not enqueue message.");
		return qtrue;
	}
}

static void Irc_Proto_RefillBucket(void) {
	// calculate token refill
	const double messageBucketSize = Cvar_GetFloatValue(irc_messageBucketSize);
	const double characterBucketSize = Cvar_GetFloatValue(irc_characterBucketSize);
	const double messageBucketRate = Cvar_GetFloatValue(irc_messageBucketRate);
	const double characterBucketRate = Cvar_GetFloatValue(irc_characterBucketRate);
	const quint64 micros = IRC_IMPORT.Microseconds();
	const quint64 micros_delta = micros - irc_bucket.last_refill;
	const double msg_delta = (micros_delta * messageBucketRate) / 1000000;
	const double msg_new = irc_bucket.message_token + msg_delta;
	const double char_delta = (micros_delta * characterBucketRate) / 1000000;
	const double char_new = irc_bucket.character_token + char_delta;
	// refill token (but do not exceed maximum)
	irc_bucket.message_token = min(msg_new, messageBucketSize);
	irc_bucket.character_token = min(char_new, characterBucketSize);
	// set timestamp so next refill can calculate delta
	irc_bucket.last_refill = micros;
}

static qboolean Irc_Proto_DrainBucket(void) {
	const double characterBucketBurst = Cvar_GetFloatValue(irc_characterBucketBurst);
	qboolean status = qfalse;
	irc_bucket_message_t *msg;
	// remove messages whose size exceed our burst size (we can not send them)
	for (
		msg = irc_bucket.first_msg;
		msg && msg->msg_len > characterBucketBurst;
		msg = irc_bucket.first_msg
	) {
		irc_bucket_message_t * const next = msg->next;
		// update bucket sizes
		--irc_bucket.message_size;
		irc_bucket.character_size -= msg->msg_len;
		// free message
		Irc_MemFree(msg->msg);
		// dequeue message
		irc_bucket.first_msg = next;
	}
	// send burst of remaining messages
	for (
		msg = irc_bucket.first_msg;
		msg && !status && irc_bucket.message_token >= 1.0 && msg->msg_len <= irc_bucket.character_token;
		msg = irc_bucket.first_msg
	) {
		// send message
		status = Irc_Net_Send(irc_sock, msg->msg, msg->msg_len);
		--irc_bucket.message_token;
		irc_bucket.character_token -= msg->msg_len;
		// dequeue message
		irc_bucket.first_msg = msg->next;
		// update bucket sizes
		--irc_bucket.message_size;
		irc_bucket.character_size -= msg->msg_len;
		// free message
		Irc_MemFree(msg->msg);
		Irc_MemFree(msg);
	}
	return status;
}
