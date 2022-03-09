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
	fd_set rfds, wfds;
	struct timeval timeout;
	int ret;
	int recon_cnt;
	int err;
	socklen_t len;

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

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_SET(sockfd, &rfds);
	FD_SET(sockfd, &wfds);

	timeout.tv_sec  = 10;
	timeout.tv_usec = 0;

	recon_cnt = 5;
	do {
		CLIENT_PRINT("wait connect result...");
		ret = select(sockfd+1, &rfds, &wfds, NULL, &timeout);
		if (ret < 0) {
			CLIENT_PRINT("Connect select failed, %s", strerror(errno));
			ret = -CLIENT_ERRNO;
			goto label_client_connect_server;
		} else if (ret == 0) {
			CLIENT_PRINT("Connect select timeout");
			ret = -CLIENT_ERRNO;
			goto label_client_connect_server;
		}

		if (FD_ISSET(sockfd, &rfds) || FD_ISSET(sockfd, &wfds)) {
			if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
				CLIENT_PRINT("get socket error info failed, %s", strerror(errno));
				ret = -CLIENT_ERRNO;
				continue;
			}
			if (err != 0) {
				CLIENT_PRINT("connect failed, %s", strerror(errno));
				ret = -CLIENT_ERRNO;
				continue;
			}

			connect(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
			err = errno;
			if (err == EISCONN) {
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
 * @para[in] sockfd		file descriptor
 * @para[in] sbuf		send buff pointer
 * @para[in] buff_len	send buff size
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
 * @para[in] sockfd		file descriptor
 * @para[in] sbuf		send buff pointer
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
			CLIENT_PRINT("read failed, %s", strerror(errno));
			return -CLIENT_ERRNO;
		} else if (ret == 0) {
			CLIENT_PRINT("server closed connection");
			return 0;
		}
		rlen += ret;
	} while ((ret > 0) && (rlen < buff_len));

	CLIENT_PRINT("RX> %s", sbuf->data);

	return rlen;
}

int main(int argc, char *argv[])
{
	struct timeval timeout;
	const char *ip_str;
	const char *port_str;
	struct common_buff *buff;
	fd_set fds, rmask;
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

	FD_ZERO(&rmask);
	FD_ZERO(&fds);
    FD_SET(fileno(stdin), &fds); /* stdin can also be monitored using select */
    FD_SET(sockfd, &fds);

	timeout.tv_sec = 5;			/* wait 5s */
	timeout.tv_usec = 0;
	while (1) {
		rmask = fds;			/* reset rmask */
		ret = select(sockfd+1, &rmask, NULL, NULL, &timeout);
		if (ret < 0) {
			CLIENT_PRINT("select failed, %s", strerror(errno));
			ret = -CLIENT_ERRNO;
			break;
		} else if (ret == 0) {
			timeout.tv_sec = 5;	/* reset timeout value */
			timeout.tv_usec = 0;
			/* CLIENT_PRINT("select timeout..."); */
			continue;
		} else {
			if (FD_ISSET(fileno(stdin), &rmask)) {
				if (client_send_message(sockfd, buff, blen) < 0) {
					break;
				}

				if (strcmp((const char *)buff->data, "quit") == 0) {
					CLIENT_PRINT("ready to quit...");
					FD_CLR(fileno(stdin), &fds);
					if (shutdown(sockfd, 1)) {
						CLIENT_PRINT("shutdown failed, %s", strerror(errno));
						ret = -CLIENT_ERRNO;
						break;
					}
					break;
				}
			}

			if (FD_ISSET(sockfd, &rmask)) {
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
