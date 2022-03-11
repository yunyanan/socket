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

#include "common.h"

#define LISTENQ						20
#define MAX_CLIENTS					20

#define SERVER_ERRNO				__LINE__
#define SERVER_PRINT(_fmt, ...)		printf("[%04d] "_fmt"\n", __LINE__, ##__VA_ARGS__);

struct client_connect_info {
	int fd;
	struct sockaddr_in clientaddr;
};

/**
 * Listen socket connection
 *
 * @param[in] port_str	port string
 *
 * @return On success, return the client connection fd.
 *		   On error, negative number of the error line number
 */
static int server_listen_connection(const char *port_str)
{
	struct sockaddr_in servaddr;
	uint16_t port;
	int sockfd;
	int flags;
	int ret;
	int on = 1;

	/* Creating a socket descriptor  */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		SERVER_PRINT("create socket failed, %s", strerror(errno));
		return -SERVER_ERRNO;
	}
	SERVER_PRINT("create ok");

	flags = fcntl(sockfd, F_GETFL, 0);
	/* set non-blocking mode */
	fcntl(sockfd, F_SETFL, flags|O_NONBLOCK);

	port = atoi(port_str);
	bzero(&servaddr, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

	ret = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		SERVER_PRINT("bind port failed, %s", strerror(errno));
		ret = -SERVER_ERRNO;
		goto label_server_listen_connection;
	}

	ret = listen(sockfd, LISTENQ);
	if (ret < 0) {
		SERVER_PRINT("listen failed, %s", strerror(errno));
		ret = -SERVER_ERRNO;
		goto label_server_listen_connection;
	}

	return sockfd;
label_server_listen_connection:
	if (sockfd > 0) {
		close(sockfd);
		sockfd = -1;
	}
	return ret;
}

/**
 * Receive a message from the client
 *
 * @param[in] clientfd	client connection file descriptor
 * @param[in] sbuf		recv buff pointer
 * @param[in] buff_len	recv buff size
 *
 * @return On success, return the length of the sent.
 */
static int server_recv_message(int clientfd, struct common_buff *sbuf, uint16_t buff_len)
{
	char *rptr;
	uint32_t rlen;
	int ret;

	rptr = (char *)sbuf;
	rlen = 0;
	do {
		ret = read(clientfd, &rptr[rlen], buff_len - rlen);
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

	SERVER_PRINT("RX[%04d]> %s", rlen, sbuf->data);

	return rlen;
}

/**
 * Send a message to the client
 *
 * @param[in] clientfd	client connection file descriptor
 * @param[in] sbuf		send buff pointer
 * @param[in] buff_len	send buff size
 *
 * @return On success, return the length of the sent.
 */
static int server_send_message(int clientfd, struct common_buff *sbuf, uint16_t buff_len)
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
	ret = write(clientfd, sbuf, slen);
	if (ret < 0) {
		/* we failed */
		SERVER_PRINT("write failed, %s", strerror(errno));
		return -SERVER_ERRNO;
	}
	SERVER_PRINT("TX[%04d]> %s", ret, sbuf->data);

	return ret;
}

/**
 * Select the client number to send the message to
 *
 * @param[in] client_info	Client Connection Info
 *
 * @return On success, return the index of the client
 */
static int server_select_client(struct client_connect_info *client_info)
{
	char index[5+1] = {0};
	int i, len;

	fgets(index, sizeof(char)*5, stdin);
	len = strlen(index);
	if (len > 0) {
		index[len - 1] = '\0';	/* delete \n */
	}

	i = atoi(index);
	if ((i >= MAX_CLIENTS) || (client_info[i].fd <= 0)) {
		SERVER_PRINT("input error.");
		return -SERVER_ERRNO;
	}

	return i;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in clientaddr;
	struct client_connect_info client_info[MAX_CLIENTS];
	struct common_buff *buff;
	struct timeval timeout;
	const char *port_str;
	uint16_t blen;
	fd_set fds;
	int sockfd, maxfd;
	int i, connect_cnt, check_cnt, close_cnt, ret;

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

	port_str = argv[1];
	SERVER_PRINT("port: %s", port_str);

	sockfd = server_listen_connection(port_str);
	if (sockfd < 0) {
		SERVER_PRINT("accept client connection failed");
		free(buff);
		return -SERVER_ERRNO;
	}

	connect_cnt = 0;
	memset(client_info, 0x00, sizeof(struct client_connect_info) * MAX_CLIENTS);
	for (i=0; i<MAX_CLIENTS; i++) {
		client_info[i].fd = -1;
	}

	timeout.tv_sec = 5;			/* wait 5s */
	timeout.tv_usec = 0;
	while (1) {
		FD_ZERO(&fds);
		FD_SET(fileno(stdin), &fds);
		FD_SET(sockfd, &fds);
		maxfd = sockfd;

		SERVER_PRINT("Select a client to send a message:");
		for (i=0,check_cnt=0; (i<MAX_CLIENTS) && (check_cnt < connect_cnt); i++) {
			if (client_info[i].fd > 0) {
				FD_SET(client_info[i].fd, &fds);
				SERVER_PRINT("Client %d: %s:%d", i, inet_ntoa(client_info[i].clientaddr.sin_addr),
							 client_info[i].clientaddr.sin_port);
				if (client_info[i].fd > maxfd) {
					maxfd = client_info[i].fd;
				}
				check_cnt++;
			}
		}
		SERVER_PRINT("---------------------------------\n");

		ret = select(maxfd+1, &fds, NULL, NULL, &timeout);
		if (ret < 0) {
			SERVER_PRINT("select failed, %s", strerror(errno));
			ret = -SERVER_ERRNO;
			break;
		} else if (ret == 0) {
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
			/* SERVER_PRINT("select timeout..."); */
			continue;
		} else {
			if (FD_ISSET(fileno(stdin), &fds)) {
				i = server_select_client(client_info);
				if (i >= 0) {
					if (server_send_message(client_info[i].fd, buff, blen) < 0) {
						close(client_info[i].fd);
						client_info[i].fd = -1;
						connect_cnt --;
					}
				}
			}

			if (FD_ISSET(sockfd, &fds)) {
				socklen_t client_len = sizeof(struct sockaddr_in);
				int connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &client_len);
				if (connfd < 0) {
					SERVER_PRINT("accept failed, %s", strerror(errno));
					ret = -SERVER_ERRNO;
					break;
				}

				if (connect_cnt >= MAX_CLIENTS) {
					SERVER_PRINT("too many connections");
					close(connfd);
					connfd = -1;
				} else {
					SERVER_PRINT("accpet a new client: %s:%d", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);
					for (i=0; i<MAX_CLIENTS; i++) {
						if (client_info[i].fd <= 0) {
							int flags = fcntl(connfd, F_GETFL, 0);
							/* set non-blocking mode */
							fcntl(connfd, F_SETFL, flags|O_NONBLOCK);

							client_info[i].fd = connfd;
							client_info[i].clientaddr = clientaddr;
							connect_cnt ++;
							break;
						}
					}
				}
			}

			for (i=0, check_cnt=0, close_cnt=0; (i<MAX_CLIENTS) && (check_cnt < connect_cnt); i++) {
				if ((client_info[i].fd > 0) && (FD_ISSET(client_info[i].fd, &fds))) {
					check_cnt++;
					SERVER_PRINT("From client %s:%d.", inet_ntoa(client_info[i].clientaddr.sin_addr),
								 client_info[i].clientaddr.sin_port);
					if (server_recv_message(client_info[i].fd, buff, blen) <= 0) {
						close(client_info[i].fd);
						FD_CLR(client_info[i].fd, &fds);
						client_info[i].fd = -1;
						close_cnt++;
						SERVER_PRINT("connect %s:%d closed.", inet_ntoa(client_info[i].clientaddr.sin_addr),
									 client_info[i].clientaddr.sin_port);
					}
				}
			}
			connect_cnt -= close_cnt;
		}
	}

	for (i=0; i<MAX_CLIENTS; i++) {
		if (client_info[i].fd > 0) {
			close(client_info[i].fd);
			client_info[i].fd = -1;
		}
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
