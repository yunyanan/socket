#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/un.h>

#include "common.h"

#define CLIENT_ERRNO				__LINE__
#define CLIENT_PRINT(_fmt, ...)		printf("[%04d] "_fmt"\n", __LINE__, ##__VA_ARGS__);

/**
 * Connect to the server
 *
 * @param[in] local_path	Full path to local socket file
 *
 * @return On success, a file descriptor for the new socket is returned.
 *		   On error, negative number of the error line number
 */
static int client_connect_server(const char *local_path)
{
    struct sockaddr_un servaddr;
	int sockfd;
	int flags;
	int ret;

	sockfd = -1;
	/* Creating a socket descriptor  */
	sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (sockfd < 0) {
		CLIENT_PRINT("create socket failed, %s", strerror(errno));
		return -CLIENT_ERRNO;
	}
	CLIENT_PRINT("create ok");

	/* set non-blocking mode */
	flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags|O_NONBLOCK);

	bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, local_path);

	ret = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_un));
	if (ret < 0) {
		CLIENT_PRINT("Connect failed, %s", strerror(errno));
		close(sockfd);
		return -CLIENT_ERRNO;
	}

	CLIENT_PRINT("connect ok");
	return sockfd;
}

/**
 * Send a message to the server
 *
 * @param[in] sockfd	socket file descriptor
 * @param[in] sbuf		send buff pointer
 * @param[in] buff_len	send buff size
 *
 * @return On success, return the length of the sent.
 */
static int client_send_message(int sockfd, struct common_buff *sbuf, uint16_t buff_len)
{
	uint32_t slen;
	int ret;

	/* clear send buff */
	memset(sbuf->data, 0x00, buff_len);

	/* get input from stdin  */
	fgets((char *)sbuf->data, buff_len, stdin);
	slen = strlen((char *)sbuf->data);
	if (slen == 0) {
		CLIENT_PRINT("Input is empty");
		return 0;
	}
	slen -= 1;
	sbuf->data[slen] = '\0'; /* delete \n */

	/* send to server */
	ret = write(sockfd, sbuf->data, slen);
	if (ret < 0) {
		/* we failed */
		CLIENT_PRINT("write failed, %s", strerror(errno));
		return -CLIENT_ERRNO;
	}
	CLIENT_PRINT("TX[%04d]> %s", ret, sbuf->data);
	/* CLIENT_PRINT("send %d bytes data", ret); */

	return ret;
}

/**
 * Receive a message from the server
 *
 * @param[in] sockfd	socket file descriptor
 * @param[in] rbuf		recv buff pointer
 * @param[in] buff_len	recv buff size
 *
 * @return On success, return the length of the sent.
 */
static int client_recv_message(int sockfd, struct common_buff *rbuf, uint16_t buff_len)
{
	char *ptr;
	uint32_t rlen;
	int ret;

	ptr = (char *)rbuf;
	rlen = 0;
	do {
		ret = read(sockfd, &ptr[rlen], buff_len - rlen);
		if (ret < 0) {
			if (errno != EAGAIN) {
				CLIENT_PRINT("read failed, %s", strerror(errno));
				return -CLIENT_ERRNO;
			}
			break;
		} else if (ret == 0) {
			CLIENT_PRINT("server closed connection");
			return 0;
		}
		rlen += ret;
	} while ((ret > 0) && (rlen < buff_len));

	CLIENT_PRINT("RX[%04d]> %s", rlen, rbuf->data);

	return rlen;
}

int main(int argc, char *argv[])
{
	struct common_buff *buff;
	struct epoll_event epev;
	struct epoll_event events[2];
	char *local_path;
	uint32_t timeout;
	uint16_t blen;
	int sockfd, epfd;
	int i, ret;

	if (argc < 2) {
		CLIENT_PRINT("usage: ./client loacl_path");
		return -CLIENT_ERRNO;
	}

	sockfd = epfd = -1;
	blen = sizeof(struct common_buff);
	buff = (struct common_buff *)malloc(blen);
	if (!buff) {
		CLIENT_PRINT("get %d bytes buff memory failed", blen);
		return -CLIENT_ERRNO;
	}

	local_path = argv[1];
	CLIENT_PRINT("path: %s", local_path);

	sockfd = client_connect_server(local_path);
	if (sockfd < 0) {
		CLIENT_PRINT("connect server failed, %d", sockfd);
		ret = -CLIENT_ERRNO;
		goto label_main_exit;
	}

	epfd = epoll_create(2);
	if (epfd < 0) {
		CLIENT_PRINT("epoll failed, %s", strerror(errno));
		ret = -CLIENT_ERRNO;
		goto label_main_exit;
	}

	timeout = 10*1000;
	memset(&epev, 0x00, sizeof(struct epoll_event));
	epev.events = EPOLLIN;
	epev.data.fd = fileno(stdin);	/* stdin can also be monitored using epoll */
	epoll_ctl(epfd, EPOLL_CTL_ADD, fileno(stdin), &epev);

	epev.events = EPOLLIN;
	epev.data.fd = sockfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &epev);

	memset(events, 0x00, sizeof(struct epoll_event) * 2);
	while (1) {
		ret = epoll_wait(epfd, events, 2, timeout);
		if (ret < 0) {
			CLIENT_PRINT("epoll failed, %s", strerror(errno));
			ret = -CLIENT_ERRNO;
			break;
		} else if (ret == 0) {
			/* CLIENT_PRINT("epoll timeout..."); */
			continue;
		} else {
			for (i=0; i<ret; i++) {
				if (events[i].events & EPOLLIN) {
					if (events[i].data.fd == fileno(stdin)) {
						if (client_send_message(sockfd, buff, blen) < 0) {
							goto label_main_exit;
						}
						if (strcmp((const char *)buff->data, "quit") == 0) {
							CLIENT_PRINT("ready to quit...");
							epoll_ctl(epfd, EPOLL_CTL_DEL, fileno(stdin), NULL);
							if (shutdown(sockfd, 1)) {
								CLIENT_PRINT("shutdown failed, %s", strerror(errno));
								ret = -CLIENT_ERRNO;
							}
							goto label_main_exit;
						}
					} else if (events[i].data.fd == sockfd) {
						if (client_recv_message(sockfd, buff, blen) <= 0) {
							goto label_main_exit;
						}
					}
				}
			}
		}
	}

label_main_exit:
	if (epfd > 0) {
		close(epfd);
		epfd = -1;
	}
	if (sockfd > 0) {
		close(sockfd);
		sockfd = -1;
	}
	if (buff) {
		free(buff);
		buff = NULL;
	}

	CLIENT_PRINT("client exit...");

	return 0;
}
