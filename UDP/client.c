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

#include "common.h"

#define CLIENT_ERRNO				__LINE__
#define CLIENT_PRINT(_fmt, ...)		printf("[%04d] "_fmt"\n", __LINE__, ##__VA_ARGS__);

/**
 * Send a message to the server
 *
 * @param[in] sockfd	file descriptor
 * @param[in] sbuf		send buff pointer
 * @param[in] buff_len	send buff size
 * @param[in] servaddr	server addr info
 *
 * @return On success, return the length of the sent.
 */
static int client_send_message(int sockfd, struct common_buff *sbuf, uint16_t buff_len,
							   struct sockaddr_in *servaddr)
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
	ret = sendto(sockfd, sbuf, slen, 0, (const struct sockaddr *)servaddr, sizeof(struct sockaddr_in));
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
 * @param[in] servaddr	server addr info
 *
 * @return On success, return the length of the sent.
 */
static int client_recv_message(int sockfd, struct common_buff *rbuf, uint16_t buff_len,
							   struct sockaddr_in *servaddr)
{
	socklen_t len;
	char *ptr;
	uint32_t rlen;
	int ret;

	ptr = (char *)rbuf;
	rlen = 0;
	do {
		ret = recvfrom(sockfd, &ptr[rlen], buff_len - rlen, 0, (struct sockaddr *)servaddr, &len);
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
	struct sockaddr_in servaddr;
	const char *ip_str;
	const char *port_str;
	uint32_t timeout;
	uint16_t port;
	uint16_t blen;
	int sockfd, epfd;
	int flags;
	int i, ret;

	if (argc < 3) {
		CLIENT_PRINT("usage: ./client ip port");
		return -CLIENT_ERRNO;
	}

	blen = sizeof(struct common_buff) + DATA_MAX_LEN;
	buff = (struct common_buff *)malloc(blen);
	if (!buff) {
		CLIENT_PRINT("get %d bytes buff memory failed", blen);
		return -CLIENT_ERRNO;
	}

	sockfd = epfd = -1;

	ip_str   = argv[1];
	port_str = argv[2];
	CLIENT_PRINT("addr: %s:%s", ip_str, port_str);

	port = atoi(port_str);
	bzero(&servaddr, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip_str, &servaddr.sin_addr) <= 0) {
		CLIENT_PRINT("IP %s conversion failed, %s", ip_str, strerror(errno));
		ret = -CLIENT_ERRNO;
		goto label_main_exit;
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		CLIENT_PRINT("Create socket failed, %s", strerror(errno));
		ret = -CLIENT_ERRNO;
		goto label_main_exit;
	}

	/* set non-blocking mode */
	flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags|O_NONBLOCK);

	epfd = epoll_create(2);
	if (epfd < 0) {
		CLIENT_PRINT("epoll failed, %s", strerror(errno));
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
						if (client_send_message(sockfd, buff, blen, &servaddr) < 0) {
							goto label_main_exit;
						}

						if (strcmp((const char *)buff->data, "quit") == 0) {
							CLIENT_PRINT("ready to quit...");
							epoll_ctl(epfd, EPOLL_CTL_DEL, fileno(stdin), NULL);
							close(sockfd);
							goto label_main_exit;
						}
					} else if (events[i].data.fd == sockfd) {
						if (client_recv_message(sockfd, buff, blen, &servaddr) <= 0) {
							goto label_main_exit;
						}
					}
				}
			}
		}
	}

label_main_exit:
	if (buff) {
		free(buff);
		buff = NULL;
	}
	if (sockfd > 0) {
		close(sockfd);
		sockfd = -1;
	}
	if (epfd > 0) {
		close(epfd);
		epfd = -1;
	}

	CLIENT_PRINT("client exit...");

	return 0;
}
