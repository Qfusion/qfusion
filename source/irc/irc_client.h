#ifndef IRC_CLIENT_H
#define IRC_CLIENT_H

// listens to irc_connected
void Irc_Client_Connected_f(void *connected);

// draws "chanmsg: <chat_buffer>"
void Irc_Client_DrawNotify(const char *target, const char *chat_buffer, size_t chat_bufferlen);

// draws the ingame chat window
void Irc_Client_DrawIngameWindow();

// Externalized dynvars and cvars
extern cvar_t *irc_window;
extern cvar_t *irc_windowLines;
extern cvar_t *irc_windowWidth;
extern cvar_t *dedicated;

#endif
