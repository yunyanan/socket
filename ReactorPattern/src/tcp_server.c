#include "common.h"
#include "log.h"

#define TCPSERV_ERRNO				__LINE__

int tcp_server(uint16_t port)
{
	struct sockaddr_in servaddr;
	struct sockaddr_in clientaddr;
	socklen_t client_len;
	uint16_t port;
	int sockfd, connfd;
	int ret, on;

	/* Creating a socket descriptor  */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		LOGERR("create socket failed, %s", strerror(errno));
		return -TCPSERV_ERRNO;
	}
	LOGRUN("create ok");

	port = atoi(port_str);
	bzero(&servaddr, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

	ret = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		LOGERR("bind port failed, %s", strerror(errno));
		ret = -TCPSERV_ERRNO;
		goto label_tcp_server;
	}

	ret = listen(sockfd, LISTENQ);
	if (ret < 0) {
		LOGERR("listen failed, %s", strerror(errno));
		ret = -TCPSERV_ERRNO;
		goto label_tcp_server;
	}

	client_len = sizeof(struct sockaddr_in);
	connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &client_len);
	if (connfd < 0) {
		LOGERR("accept failed, %s", strerror(errno));
		ret = -TCPSERV_ERRNO;
		goto label_tcp_server;
	}
	LOGDBG("accpet a new client: %s:%d", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);
	close(sockfd);
	sockfd = -1;


}
