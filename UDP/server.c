#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>

#include "common.h"

#define SERVER_ERRNO				__LINE__
#define SERVER_PRINT(_fmt, ...)		printf("[%04d] "_fmt"\n", __LINE__, ##__VA_ARGS__);

/**
 * Receive a message from the client
 *
 * @param[in] sockfd	client connection file descriptor
 * @param[in] rbuf		recv buff pointer
 * @param[in] buff_len	recv buff size
 * @param[in] cliaddr	client addr info
 *
 * @return On success, return the length of the sent.
 */
static int server_recv_message(int sockfd, struct common_buff *rbuf, uint16_t buff_len,
							   struct sockaddr_in *cliaddr)
{
	socklen_t len;
	char *rptr;
	uint32_t rlen;
	int ret;

	rptr = (char *)rbuf;
	rlen = 0;

	len = sizeof(struct sockaddr_in);
	do {
		ret = recvfrom(sockfd, &rptr[rlen], buff_len - rlen, 0, (struct sockaddr *)cliaddr, &len);
		if (ret < 0) {
			if (errno != EAGAIN) {
				SERVER_PRINT("read failed, %d, %s", errno, strerror(errno));
				return -SERVER_ERRNO;
			}
			break;
		} else if (ret == 0) {
			SERVER_PRINT("client closed connection");
			return 0;
		}
		rlen += ret;
	} while (ret > 0);

	SERVER_PRINT("RX[%04d]> %s", rlen, rbuf->data);

	return rlen;
}

/**
 * Send a message to the client
 *
 * @param[in] sockfd	client connection file descriptor
 * @param[in] sbuf		send buff pointer
 * @param[in] buff_len	send buff size
 * @param[in] cliaddr	client addr info
 *
 * @return On success, return the length of the sent.
 */
static int server_send_message(int sockfd, struct common_buff *sbuf, uint16_t buff_len,
							   struct sockaddr_in *cliaddr)
{
	uint32_t slen;
	int ret;

	/* clear send buff */
	memset(sbuf->data, 0x00, buff_len);

	/* get input from stdin  */
	fgets((char *)sbuf->data, buff_len, stdin);
	slen = strlen((char *)sbuf->data);
	if (slen == 0) {
		SERVER_PRINT("Input is empty");
		return 0;
	}
	slen -= 1;
	sbuf->data[slen] = '\0'; /* delete \n */

	/* send to server */
	ret = sendto(sockfd, sbuf, slen, 0, (const struct sockaddr *)cliaddr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		/* we failed */
		SERVER_PRINT("write failed, %s", strerror(errno));
		return -SERVER_ERRNO;
	}
	SERVER_PRINT("TX[%04d]> %s", ret, sbuf->data);

	return ret;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in servaddr;
	struct sockaddr_in cliaddr;
	struct common_buff *buff;
	struct epoll_event epev;
	struct epoll_event events[2];
	const char *port_str;
	uint32_t timeout;
	uint16_t port;
	uint16_t blen;
	int sockfd, epfd;
	int i, ret, flags, on;

	if (argc < 2) {
		SERVER_PRINT("usage: ./server port");
		return -SERVER_ERRNO;
	}

	blen = sizeof(struct common_buff);
	buff = (struct common_buff *)malloc(blen);
	if (!buff) {
		SERVER_PRINT("get %d bytes buff memory failed", blen);
		return -SERVER_ERRNO;
	}

	sockfd = epfd = -1;

	port_str = argv[1];
	SERVER_PRINT("port: %s", port_str);

	port = atoi(port_str);
	bzero(&cliaddr, sizeof(struct sockaddr_in));
	bzero(&servaddr, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Creating a socket descriptor  */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		SERVER_PRINT("create socket failed, %s", strerror(errno));
		return -SERVER_ERRNO;
	}
	SERVER_PRINT("create ok");

	flags = fcntl(sockfd, F_GETFL, 0);
	/* set non-blocking mode */
	fcntl(sockfd, F_SETFL, flags|O_NONBLOCK);

	on = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

	ret = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		SERVER_PRINT("bind port failed, %s", strerror(errno));
		ret = -SERVER_ERRNO;
		goto label_main_exit;
	}

	epfd = epoll_create(2);
	if (epfd < 0) {
		SERVER_PRINT("epoll create failed, %s", strerror(errno));
		goto label_main_exit;
	}

	timeout = 10 * 1000;
	memset(&epev, 0x00, sizeof(struct epoll_event));
	epev.events = EPOLLIN;
	epev.data.fd = fileno(stdin);	/* stdin can also be monitored using poll */
	epoll_ctl(epfd, EPOLL_CTL_ADD, fileno(stdin), &epev);

	epev.events = EPOLLIN;
	epev.data.fd = sockfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &epev);

	memset(events, 0x00, (sizeof(struct epoll_event) * 2));
	while (1) {
		ret = epoll_wait(epfd, events, 2, timeout);
		if (ret < 0) {
			SERVER_PRINT("epoll failed, %s", strerror(errno));
			ret = -SERVER_ERRNO;
			break;
		} else if (ret == 0) {
			/* SERVER_PRINT("epoll timeout..."); */
			continue;
		} else {
			for (i=0; i<ret; i++) {
				if (events[i].events & EPOLLIN) {
					if (events[i].data.fd == fileno(stdin)) { /* stdin */
						if (server_send_message(sockfd, buff, blen, &cliaddr) < 0) {
							goto label_main_exit;
						}
						SERVER_PRINT("send to client: %s:%d", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
					} else if (events[i].data.fd == sockfd) {
						if (server_recv_message(sockfd, buff, blen, &cliaddr) <= 0) {
							goto label_main_exit;
						}
						SERVER_PRINT("receive from client: %s:%d", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
					}
				}
			}
		}
	}

label_main_exit:
	if (epfd > 0) {
		close(epfd);
	}
	if (sockfd > 0) {
		close(sockfd);
		sockfd = -1;
	}
	if (buff) {
		free(buff);
		buff = NULL;
	}

	SERVER_PRINT("server exit ...");

	return 0;
}
