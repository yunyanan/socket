
#include "log.h"

int tcp_client(const char *ip, int port)
{
	struct sockaddr_in servaddr;
	int sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		LOGERR("create tcp client sockfd failed, %s", strerror(errno));
		return -TCPCLI_ERRNO;
	}
	LOGRUN("sockte %d create ok", sockfd);

	bzero(&servaddr, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
		LOGERR("IP %s conversion failed, %s", ip_str, strerror(errno));
		close(sockfd);
		return -TCPCLI_ERRNO;
	}

	/* Connect to the server, three handshakes right here */
	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in)) < 0) {
		LOGERR("Connect failed, %s", strerror(errno));
		close(sockfd);
		return -TCPCLI_ERRNO;
	}
	LOGDBG("connect ok");

	return socket_fd;
}
