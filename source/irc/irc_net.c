#include "irc_common.h"
#include "irc_net.h"

#ifdef _WIN32
#	include <winerror.h>
#else
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <sys/socket.h>
#	include <netdb.h>
#	include <unistd.h>
#	include <fcntl.h>
#	include <errno.h>
#endif

qboolean Irc_Net_Connect(const char *host, unsigned short port, irc_socket_t *sock) {
	qboolean failed = qtrue;
	*sock = socket(PF_INET, SOCK_STREAM, 0);
	if (*sock >= 0) {
		struct sockaddr_in addr;
		struct hostent *he;
		memset(&addr, 0, sizeof(addr));
		he = gethostbyname(host);		// DNS lookup
		if (he) {
			int status;
			// convert host entry to sockaddr_in
			addr.sin_port = htons(port);
			addr.sin_addr.s_addr = ((struct in_addr*) he->h_addr)->s_addr;
			addr.sin_family = AF_INET;
			status = connect(*sock, (const struct sockaddr*) &addr, sizeof(addr));
			if (!status) {
				// connection successful
				failed = qfalse;
			} else {
				strcpy(IRC_ERROR_MSG, "Connection refused");
#ifdef _WIN32
				closesocket(*sock);
#else
				close(*sock);
#endif
			}
		} else {
			strcpy(IRC_ERROR_MSG, "Unknown host");
#ifdef _WIN32
			closesocket(*sock);
#else
			close(*sock);
#endif
		}
	} else
		strcpy(IRC_ERROR_MSG, "Could not create socket");

	if (!failed) {
		int status;
#ifdef _WIN32
		unsigned long one = 1;
		status = ioctlsocket(*sock, FIONBIO, &one);
#else
		status = fcntl(*sock, F_SETFL, O_NONBLOCK) == -1;
#endif
		if (status) {
			strcpy(IRC_ERROR_MSG, "Could not set non-blocking socket mode");
			failed = qtrue;
		}
	}

	return failed;
}

qboolean Irc_Net_Disconnect(irc_socket_t sock) {
#ifdef _WIN32
	return closesocket(sock) < 0;
#else
	return close(sock) == 0;
#endif
}

qboolean Irc_Net_Send(irc_socket_t sock, const char *msg, size_t msg_len) {
	int sent;
	assert(msg);
	sent = send(sock, msg, (int) msg_len, 0);
	if (sent >= 0)
		return qfalse;
	else {
		strcpy(IRC_ERROR_MSG, "send failed");
		return qtrue;
	}
}

qboolean Irc_Net_Receive(irc_socket_t sock, char *buf, size_t buf_len, int *recvd) {
	assert(buf);
	assert(recvd);
	*recvd = recv(sock, buf, (int) buf_len, 0);
#ifdef _WIN32
	if (*recvd < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
		*recvd = 0;
#else
	if (*recvd < 0 && errno == EAGAIN)
		*recvd = 0;
#endif
	if (*recvd >= 0)
		return qfalse;
	else {
		strcpy(IRC_ERROR_MSG, "recv failed");
		return qtrue;
	}
}
