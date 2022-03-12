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
#include <sys/un.h>

#include "common.h"

#define LISTENQ						20
#define MAX_CLIENTS					20

#define SERVER_ERRNO				__LINE__
#define SERVER_PRINT(_fmt, ...)		printf("[%04d] "_fmt"\n", __LINE__, ##__VA_ARGS__);

struct client_connect_info {
	int fd;
};

/**
 * Listen socket connection
 *
 * @param[in] local_path
 *
 * @return On success, return the client connection fd.
 *		   On error, negative number of the error line number
 */
static int server_listen_connection(const char *local_path)
{
#if 1
    struct sockaddr_un servaddr;
    int sockfd;
	int flags;
	int ret;

	sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (sockfd < 0) {
		SERVER_PRINT("create socket failed, %s", strerror(errno));
		return -SERVER_ERRNO;
	}
	SERVER_PRINT("create ok");

	flags = fcntl(sockfd, F_GETFL, 0);
	/* set non-blocking mode */
	fcntl(sockfd, F_SETFL, flags|O_NONBLOCK);

    unlink(local_path);
    bzero(&servaddr, sizeof(struct sockaddr_un));
    servaddr.sun_family = AF_LOCAL;
    strcpy(servaddr.sun_path, local_path);

	ret = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_un));
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

#else
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
#endif
}

/**
 * Receive a message from the client
 *
 * @param[in] clientfd	client connection file descriptor
 * @param[in] rbuf		recv buff pointer
 * @param[in] buff_len	recv buff size
 *
 * @return On success, return the length of the sent.
 */
static int server_recv_message(int clientfd, struct common_buff *rbuf, uint16_t buff_len)
{
	char *rptr;
	uint32_t rlen;
	int ret;

	rptr = (char *)rbuf;
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

	SERVER_PRINT("RX[%04d]> %s", rlen, rbuf->data);

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
	struct client_connect_info client_info[MAX_CLIENTS];
	struct sockaddr_un clientaddr;
	struct common_buff *buff;
	struct epoll_event epev;
	struct epoll_event events[MAX_CLIENTS];
	socklen_t client_len;
	uint32_t timeout;
	uint16_t blen;
	char *local_path;
	int sockfd, connfd, epfd;
	int i, t, connect_cnt, check_cnt, ret;

	if (argc < 2) {
		SERVER_PRINT("usage: ./server local_path");
		return -SERVER_ERRNO;
	}

	local_path = argv[1];
	SERVER_PRINT("local path: %s", local_path);

	blen = sizeof(struct common_buff);
	buff = (struct common_buff *)malloc(blen);
	if (!buff) {
		SERVER_PRINT("get %d bytes buff memory failed", blen);
		return -SERVER_ERRNO;
	}

	sockfd = server_listen_connection(local_path);
	if (sockfd < 0) {
		SERVER_PRINT("accept client connection failed");
		ret = -SERVER_ERRNO;
		goto label_main_exit;
	}

	client_len = sizeof(struct sockaddr_in);
	connect_cnt = 0;
	memset(client_info, 0x00, sizeof(struct client_connect_info) * MAX_CLIENTS);

	epfd = epoll_create(2);
	if (epfd < 0) {
		SERVER_PRINT("epoll failed, %s", strerror(errno));
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

	memset(events, 0x00, (sizeof(struct epoll_event) * MAX_CLIENTS));
	while (1) {
		SERVER_PRINT("Select a client to send a message:");
		for (i=0,check_cnt=0; (i<MAX_CLIENTS) && (check_cnt < connect_cnt); i++) {
			if (client_info[i].fd > 0) {
				SERVER_PRINT("Client %d: %d", i, client_info[i].fd);
				check_cnt++;
			}
		}
		SERVER_PRINT("---------------------------------\n");

		ret = epoll_wait(epfd, events, MAX_CLIENTS, timeout);
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
						t = server_select_client(client_info);
						if (t >= 0) {
							if (server_send_message(client_info[t].fd, buff, blen) < 0) {
								epoll_ctl(epfd, EPOLL_CTL_DEL, client_info[t].fd, NULL);
								close(client_info[t].fd);
								client_info[t].fd = -1;
								connect_cnt --;
							}
						}
					} else if (events[i].data.fd == sockfd) {
						connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &client_len);
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
							SERVER_PRINT("accpet a new client, fd: %d", connfd);
							for (t=0; t<MAX_CLIENTS; t++) {
								if (client_info[t].fd <= 0) {
									int flags = fcntl(connfd, F_GETFL, 0);
									/* set non-blocking mode */
									fcntl(connfd, F_SETFL, flags|O_NONBLOCK);

									epev.events = EPOLLIN;
									epev.data.fd = connfd;
									epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &epev);

									client_info[t].fd = connfd;
									connect_cnt ++;
									break;
								}
							}
						}
					} else {
						for (t=0; t<MAX_CLIENTS; t++) { /* This loop is just to print out the log */
							if ((events[i].data.fd > 0) && (events[i].data.fd == client_info[t].fd)) {
								SERVER_PRINT("From client %d: %d.", t, client_info[t].fd);
								if (server_recv_message(events[i].data.fd, buff, blen) <= 0) {
									epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
									SERVER_PRINT("connect %d:%d closed.", t, client_info[t].fd);
									close(events[i].data.fd);
									client_info[t].fd = -1;
									connect_cnt --;
								}
								break;
							}
						}
					}
				}
			}
		}
	}

label_main_exit:
	for (i=0; i<MAX_CLIENTS; i++) {
		if (client_info[i].fd > 0) {
			close(client_info[i].fd);
			client_info[i].fd = -1;
		}
	}
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
