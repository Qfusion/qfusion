#ifndef IRC_NET_H
#define IRC_NET_H

#ifdef _WIN32
#	include <winsock2.h>
	typedef SOCKET irc_socket_t;
#else
	typedef int irc_socket_t;
#endif

bool Irc_Net_Connect(const char *host, unsigned short port, irc_socket_t *sock);
bool Irc_Net_Disconnect(irc_socket_t sock);

bool Irc_Net_Send(irc_socket_t sock, const char *msg, size_t msg_len);
bool Irc_Net_Receive(irc_socket_t sock, char *buf, size_t buf_len, int *recvd);

#endif
