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

#include "common.h"

#define CLIENT_ERRNO				__LINE__
#define CLIENT_PRINT(_fmt, ...)		printf("[%04d] "_fmt"\n", __LINE__, ##__VA_ARGS__);

/**
 * Connect to the server
 *
 * @param[in] ip_str	ip address string
 * @param[in] port_str	port string
 *
 * @return On success, a file descriptor for the new socket is returned.
 *		   On error, negative number of the error line number
 */
static int client_connect_server(const char *ip_str, const char *port_str)
{
	struct sockaddr_in servaddr;
	uint16_t port;
	int sockfd;
	int flags;
	struct pollfd pfd;
	uint32_t timeout;
	int ret;
	int recon_cnt;

	/* Creating a socket descriptor  */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		CLIENT_PRINT("create socket failed, %s", strerror(errno));
		return -CLIENT_ERRNO;
	}
	CLIENT_PRINT("create ok");

	/* set non-blocking mode */
	flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags|O_NONBLOCK);

	port = atoi(port_str);
	bzero(&servaddr, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip_str, &servaddr.sin_addr) <= 0) {
		CLIENT_PRINT("IP %s conversion failed, %s", ip_str, strerror(errno));
		ret = -CLIENT_ERRNO;
		goto label_client_connect_server;
	}

	/* Connect to the server, three handshakes right here */
	ret = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
	if (ret == 0) {
		/* connect ok */
		CLIENT_PRINT("connect ok");
		return sockfd;
	} else if ((ret < 0) && (errno != EINPROGRESS)) {
		CLIENT_PRINT("Connect failed, %s", strerror(errno));
		ret = -CLIENT_ERRNO;
		goto label_client_connect_server;
	}

	memset(&pfd, 0x00, sizeof(struct pollfd));
	pfd.fd = sockfd;
	pfd.events = POLLOUT;
	timeout = 10*1000;
	recon_cnt = 5;
	do {
		CLIENT_PRINT("wait connect result...");
		ret = poll(&pfd, 1, timeout);
		if (ret < 0) {
			CLIENT_PRINT("Connect poll failed, %s", strerror(errno));
			ret = -CLIENT_ERRNO;
			goto label_client_connect_server;
		} else if (ret == 0) {
			CLIENT_PRINT("Connect poll timeout");
			ret = -CLIENT_ERRNO;
			goto label_client_connect_server;
		}

		if (pfd.revents & POLLOUT) {
			/* Re-judge the connection result */
			connect(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
			if (errno == EISCONN) {
				CLIENT_PRINT("connect ok");
				return sockfd;
			}
			CLIENT_PRINT("connect failed, retry, %s", strerror(errno));
			ret = -CLIENT_ERRNO;
		}
	} while ((--recon_cnt) > 0);

label_client_connect_server:
	if (sockfd > 0) {
		close(sockfd);
		sockfd = -1;
	}

	return ret;
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
	if (slen > 0) {
		sbuf->data[slen - 1] = '\0'; /* delete \n */
	}
	slen = strlen((char *)sbuf->data);

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
 * @param[in] sbuf		recv buff pointer
 * @param[in] buff_len	recv buff size
 *
 * @return On success, return the length of the sent.
 */
static int client_recv_message(int sockfd, struct common_buff *sbuf, uint16_t buff_len)
{
	char *ptr;
	uint32_t rlen;
	int ret;

	ptr = (char *)sbuf;
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

	CLIENT_PRINT("RX[%04d]> %s", rlen, sbuf->data);

	return rlen;
}

int main(int argc, char *argv[])
{
	const char *ip_str;
	const char *port_str;
	struct common_buff *buff;
	struct pollfd pfds[2];		/* stdin + sockfd */
	uint32_t timeout;
	uint16_t blen;
	int sockfd;
	int ret;

	if (argc < 3) {
		CLIENT_PRINT("usage: ./client ip port");
		return -CLIENT_ERRNO;
	}

	blen = sizeof(struct common_buff);
	buff = (struct common_buff *)malloc(blen);
	if (!buff) {
		CLIENT_PRINT("get %d bytes buff memory failed", blen);
		return -CLIENT_ERRNO;
	}

	ip_str   = argv[1];
	port_str = argv[2];
	CLIENT_PRINT("addr: %s:%s", ip_str, port_str);

	sockfd = client_connect_server(ip_str, port_str);
	if (sockfd < 0) {
		CLIENT_PRINT("connect server failed, %d", sockfd);
		return -CLIENT_ERRNO;
	}

	timeout = 10*1000;
	memset(pfds, 0x00, sizeof(struct pollfd)*2);
	pfds[0].fd = fileno(stdin);	/* stdin can also be monitored using poll */
	pfds[0].events = POLLIN;
	pfds[1].fd = sockfd;
	pfds[1].events = POLLIN;

	while (1) {
		ret = poll(pfds, 2, timeout);
		if (ret < 0) {
			CLIENT_PRINT("poll failed, %s", strerror(errno));
			ret = -CLIENT_ERRNO;
			break;
		} else if (ret == 0) {
			/* CLIENT_PRINT("poll timeout..."); */
			continue;
		} else {
			if (pfds[0].revents & POLLIN) {
				if (client_send_message(sockfd, buff, blen) < 0) {
					break;
				}

				if (strcmp((const char *)buff->data, "quit") == 0) {
					CLIENT_PRINT("ready to quit...");
					pfds[0].fd = -1;
					if (shutdown(sockfd, 1)) {
						CLIENT_PRINT("shutdown failed, %s", strerror(errno));
						ret = -CLIENT_ERRNO;
						break;
					}
					break;
				}
			}

			if (pfds[1].revents & POLLIN) {
				if (client_recv_message(sockfd, buff, blen) <= 0) {
					break;
				}
			}
		}
	}

	if (sockfd > 0) {
		close(sockfd);
		sockfd = -1;
	}

	CLIENT_PRINT("client exit...");

	return 0;
}
