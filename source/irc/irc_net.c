#include "irc_common.h"
#include "irc_net.h"

#ifdef _WIN32
#	include <winerror.h>
#   include <ws2tcpip.h>
#else
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <sys/socket.h>
#	include <netdb.h>
#	include <unistd.h>
#	include <fcntl.h>
#	include <errno.h>
#endif

bool Irc_Net_Connect(const char *host, unsigned short port, irc_socket_t *sock) {
	bool failed = true;
	*sock = socket(PF_INET, SOCK_STREAM, 0);
	if (*sock >= 0) {
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		struct addrinfo hints, *hostInfo;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET; // AF_INET6 for IPv6
		if (getaddrinfo(host, NULL, &hints, &hostInfo) == 0 && hostInfo != NULL) {	// DNS lookup
			int status;
			// convert host entry to sockaddr_in
			addr.sin_port = htons(port);
			addr.sin_addr.s_addr = ((struct sockaddr_in *)hostInfo->ai_addr)->sin_addr.s_addr;
			addr.sin_family = AF_INET;
			status = connect(*sock, (const struct sockaddr*) &addr, sizeof(addr));
			if (!status) {
				// connection successful
				failed = false;
			} else {
				strcpy(IRC_ERROR_MSG, "Connection refused");
#ifdef _WIN32
				closesocket(*sock);
#else
				close(*sock);
#endif
			}
			freeaddrinfo(hostInfo);
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
			failed = true;
		}
	}

	return failed;
}

bool Irc_Net_Disconnect(irc_socket_t sock) {
#ifdef _WIN32
	return closesocket(sock) < 0;
#else
	return close(sock) == 0;
#endif
}

bool Irc_Net_Send(irc_socket_t sock, const char *msg, size_t msg_len) {
	int sent;
	assert(msg);
	sent = send(sock, msg, (int) msg_len, 0);
	if (sent >= 0)
		return false;
	else {
		strcpy(IRC_ERROR_MSG, "send failed");
		return true;
	}
}

bool Irc_Net_Receive(irc_socket_t sock, char *buf, size_t buf_len, int *recvd) {
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
		return false;
	else {
		strcpy(IRC_ERROR_MSG, "recv failed");
		return true;
	}
}
