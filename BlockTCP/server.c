#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "common.h"

#define LISTENQ						20
#define SERVER_ERRNO				__LINE__
#define SERVER_PRINT(_fmt, ...)		printf("[%04d] "_fmt"\n", __LINE__, ##__VA_ARGS__);

/**
 * Accept a client connection
 *
 * @param[in] port_str	port string
 *
 * @return On success, return the client connection fd.
 *		   On error, negative number of the error line number
 */
static int server_accept_client(const char *port_str)
{
	struct sockaddr_in servaddr;
	struct sockaddr_in clientaddr;
	socklen_t client_len;
	uint16_t port;
	int sockfd, connfd;
	int ret;

	/* Creating a socket descriptor  */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		SERVER_PRINT("create socket failed, %s", strerror(errno));
		return -SERVER_ERRNO;
	}
	SERVER_PRINT("create ok");

	port = atoi(port_str);
	bzero(&servaddr, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	ret = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		SERVER_PRINT("bind port failed, %s", strerror(errno));
		ret = -SERVER_ERRNO;
		goto label_server_accept_client;
	}

	ret = listen(sockfd, LISTENQ);
	if (ret < 0) {
		SERVER_PRINT("listen failed, %s", strerror(errno));
		ret = -SERVER_ERRNO;
		goto label_server_accept_client;
	}

	client_len = sizeof(struct sockaddr_in);
	connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &client_len);
	if (connfd < 0) {
		SERVER_PRINT("accept failed, %s", strerror(errno));
		ret = -SERVER_ERRNO;
		goto label_server_accept_client;
	}
	SERVER_PRINT("accpet a new client: %s:%d", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);
	close(sockfd);
	sockfd = -1;

	return connfd;
label_server_accept_client:
	if (sockfd > 0) {
		close(sockfd);
		sockfd = -1;
	}
	return ret;
}

/**
 * Receive a message from the client
 *
 * @para[in] confd		client connection file descriptor
 * @para[in] sbuf		send buff pointer
 *
 * @return On success, return the length of the sent.
 */
static int server_recv_message(int connfd, struct common_buff *sbuf)
{
	void *rptr;
	uint32_t rlen;
	int count = 2;
	int ret;

	rptr = &sbuf->len;
	rlen = sizeof(sbuf->len);
	do {
		ret = read(connfd, rptr, rlen);
		if (ret < 0) {
			SERVER_PRINT("read failed, %s", strerror(errno));
			return -SERVER_ERRNO;
		} else if (ret == 0) {
			SERVER_PRINT("client closed connection");
			return 0;
		}

		if (count == 2) {
			sbuf->len = ntohl(sbuf->len);
			if (sbuf->len == 0) {
				/* data illegal */
				SERVER_PRINT("no data read from the client");
				break;
			} else if (sbuf->len > DATA_MAX_LEN) {
				SERVER_PRINT("data error!!!");
				return -SERVER_ERRNO;
			}
		}

		rptr = &sbuf->data;
		rlen = sbuf->len;
	} while ((--count) > 0);

	printf("RX> %s", sbuf->data);

	return rlen;

}

/**
 * Send a message to the client
 *
 * @para[in] connfd		client connection file descriptor
 * @para[in] sbuf		send buff pointer
 *
 * @return On success, return the length of the sent.
 */
static int server_send_message(int connfd, struct common_buff *sbuf)
{
	uint32_t slen;
	int ret;

	/* clear send buff */
	memset(sbuf->data, 0x00, sbuf->len);

	printf("TX> ");

	/* get input from stdin  */
	fgets((char *)sbuf->data, sbuf->len, stdin);

	sbuf->len = strlen((char *)sbuf->data);
	slen = sizeof(struct common_buff) + sbuf->len;

	sbuf->len = htonl(sbuf->len);
	/* send to server */
	ret = write(connfd, sbuf, slen);
	if (ret < 0) {
		/* we failed */
		SERVER_PRINT("write failed, %s", strerror(errno));
		return -SERVER_ERRNO;
	}

	return ret;
}

int main(int argc, char *argv[])
{
	const char *port_str;
	struct common_buff *buff;
	uint16_t blen;
	int connfd;

	if (argc < 2) {
		SERVER_PRINT("usage: ./server port");
		return -SERVER_ERRNO;
	}

	blen = sizeof(struct common_buff) + DATA_MAX_LEN;
	buff = (struct common_buff *)malloc(blen);
	if (!buff) {
		SERVER_PRINT("get %d bytes buff memory failed", blen);
		return -SERVER_ERRNO;
	}

	port_str = argv[1];
	SERVER_PRINT("port: %s", port_str);

	connfd = server_accept_client(port_str);
	if (connfd < 0) {
		SERVER_PRINT("accept client connection failed");
		return -SERVER_ERRNO;
	}

	while (1) {
		if (server_recv_message(connfd, buff) <= 0) {
			break;
		}

		buff->len = DATA_MAX_LEN;
		if (server_send_message(connfd, buff) < 0) {
			break;
		}
	}

	if (connfd > 0) {
		close(connfd);
		connfd = -1;
	}

	SERVER_PRINT("server exit ...");

	return 0;
}
