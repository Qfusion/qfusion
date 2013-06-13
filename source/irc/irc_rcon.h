#ifndef IRC_RCON_H
#define IRC_RCON_H

#define IRC_RCON_PREFIX "RCON"
#define IRC_RCON_LOGIN "LOGIN"
#define IRC_RCON_LOGOUT "LOGOUT"

// listens to irc_connected
void Irc_Rcon_Connected_f(void *connected);

extern cvar_t *irc_rcon;
extern cvar_t *irc_rconTimeout;

#endif
