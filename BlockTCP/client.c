#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "common.h"

#define CLIENT_ERRNO				__LINE__
#define CLIENT_PRINT(_fmt, ...)		printf("[%04d] "_fmt, __LINE__, ##__VA_ARGS__);

/**
 * Connect to the server
 *
 * @param[in] ip_str	ip address string
 * @param[in] port_str	port string
 *
 * @return On success, a file descriptor for the new socket is returned.
 *		   On error, negative number of the error line number
 */
static int connect_server(const char *ip_str, const char *port_str)
{
	struct sockaddr_in servaddr;
	uint16_t port;
	int sockfd;
	int ret;

	/* Creating a socket descriptor  */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		CLIENT_PRINT("create socket failed, %s\n", strerror(errno));
		return -CLIENT_ERRNO;
	}
	CLIENT_PRINT("create ok");

	port = atoi(port_str);
	bzero(&servaddr, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip_str, &servaddr.sin_addr) <= 0) {
		CLIENT_PRINT("IP %s conversion failed, %s\n", ip_str, strerror(errno));
		ret = -CLIENT_ERRNO;
		goto label_connect_server;
	}

	/* Connect to the server, three handshakes right here */
	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in)) < 0) {
		CLIENT_PRINT("Connect failed, %s\n", strerror(errno));
		ret = -CLIENT_ERRNO;
		goto label_connect_server;
	}
	CLIENT_PRINT("create ok");

	return sockfd;
label_connect_server:
	if (sockfd > 0) {
		close(sockfd);
		sockfd = -1;
	}

	return ret;
}

/**
 * Send a message to the server
 *
 * @para[in] sockfd		file descriptor
 * @para[in] sbuf		send buff pointer
 *
 * @return On success, return the length of the sent.
 */
static int send_message(int sockfd, struct common_buff *sbuf)
{
	uint32_t slen;
	int ret;

	/* clear send buff */
	memset(sbuf->data, 0x00, sbuf->len);

	printf("< ");

	/* get input from stdin  */
	fgets(sbuf->data, sbuf->len, stdin);

	sbuf->len = strlen(sbuf->data);
	slen = sizeof(struct common_buff) + sbuf->len;

	sbuf->len = htonl(sbuf->len);
	/* send to server */
	ret = write(sockfd, sbuf, slen);
	if (ret < 0) {
		/* we failed */
		CLIENT_PRINT("write failed, %s\n", strerror(errno));
		return -CLIENT_ERRNO;
	}

	return ret;
}

/**
 * Receive a message from the server
 *
 * @para[in] sockfd		file descriptor
 * @para[in] sbuf		send buff pointer
 *
 * @return On success, return the length of the sent.
 */
static int recv_message(int sockfd, struct common_buff *sbuf)
{
	void *rptr;
	uint32_t rlen;
	int count = 2;
	int ret;

	rptr = &sbuf->len;
	rlen = sizeof(sbuf->len);
	do {
		ret = read(sockfd, rptr, rlen);
		if (ret < 0) {
			CLIENT_PRINT("read failed, %s\n", strerror(errno));
			return -CLIENT_ERRNO;
		} else if (ret == 0) {
			CLIENT_PRINT("server closed connection\n");
			return 0;
		}
		sbuf->len = ntohl(sbuf->len);
		if (sbuf->len == 0) {
			/* data illegal */
			CLIENT_PRINT("no data read from the server\n");
			break;
		} else if (sbuf->len > DATA_MAX_LEN) {
			CLIENT_PRINT("data error!!!\n");
			return -CLIENT_ERRNO;
		}

		rptr = &sbuf->data;
		rlen = sbuf->len;
	} while ((count--) > 0);

	printf("> %s\n", sbuf->data);

	return rlen;
}

int main(int argc, char *argv[])
{
	const char *ip_str;
	const char *port_str;
	struct common_buff *buff;
	uint16_t blen;
	int sockfd;
	int ret;

	if (argc < 3) {
		CLIENT_PRINT("usage: ./client ip port\n");
		return -CLIENT_ERRNO;
	}

	blen = sizeof(struct common_buff) + DATA_MAX_LEN;
	buff = (struct common_buff *)malloc(blen);
	if (!buff) {
		CLIENT_PRINT("get %d bytes buff memory failed\n", blen);
		return -CLIENT_ERRNO;
	}

	ip_str   = argv[1];
	port_str = argv[2];
	CLIENT_PRINT("addr: %s:%s\n", ip_str, port_str);

	sockfd = connect_server(ip_str, port_str);
	if (sockfd < 0) {
		CLIENT_PRINT("connect server failed, %d\n", sockfd);
		return -CLIENT_ERRNO;
	}

	while (1) {
		buff->len = DATA_MAX_LEN;
		if (send_message(sockfd, buff) < 0) {
			break;
		}

		if (recv_message(sockfd, buff) <= 0) {
			break;
		}
	}

	if (sockfd > 0) {
		close(sockfd);
		sockfd = -1;
	}

	CLIENT_PRINT("client exit...\n");

	return 0;
}
