#ifndef IRC_LOGIC_H
#define IRC_LOGIC_H

#define IRC_LOGIC_VERSION "0.1"

void Irc_Logic_Connect(const char *server, unsigned short port);
void Irc_Logic_Disconnect(const char *reason);
void Irc_Logic_Connected_f(void *connected);

struct irc_channel_s;
typedef struct irc_channel_s irc_channel_t;

unsigned int Irc_Logic_NoOfChannels(void);
irc_channel_t *Irc_Logic_GetChannel(const char *name);
irc_channel_t * const *Irc_Logic_DumpChannels(void);
void Irc_Logic_FreeChannelDump(irc_channel_t * const *dump);
const char *Irc_Logic_GetChannelName(const irc_channel_t *channel);
const char *Irc_Logic_GetChannelTopic(const irc_channel_t *channel);
const trie_t *Irc_Logic_GetChannelNames(const irc_channel_t *channel);

extern dynvar_t *irc_channels;
extern cvar_t *irc_server;
extern cvar_t *irc_port;
extern cvar_t *irc_nick;
extern cvar_t *irc_ctcpReplies;
extern cvar_t *irc_perform;
extern cvar_t *irc_defaultChannel;

#endif
