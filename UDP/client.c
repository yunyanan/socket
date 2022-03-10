#include <stdio.h>

#define CLIENT_ERRNO				__LINE__
#define CLIENT_PRINT(_fmt, ...)		printf("[%04d] "_fmt"\n", __LINE__, ##__VA_ARGS__);

/**
 * Send a message to the server
 *
 * @param[in] sockfd	file descriptor
 * @param[in] sbuf		send buff pointer
 *
 * @return On success, return the length of the sent.
 */
static int client_send_message(int sockfd, struct common_buff *sbuf, struct sockaddr_in *servaddr)
{
	socklen_t servlen;
	uint32_t slen;
	int ret;

	/* clear send buff */
	memset(sbuf->data, 0x00, sbuf->len);

	/* get input from stdin  */
	fgets((char *)sbuf->data, sbuf->len, stdin);
	sbuf->len = strlen((char *)sbuf->data);
	if (sbuf->len > 0) {
		sbuf->data[sbuf->len - 1] = '\0'; /* delete \n */
	}
	sbuf->len = strlen((char *)sbuf->data);
	slen = sizeof(struct common_buff) + sbuf->len;

	sbuf->len = htonl(sbuf->len);
	servlen = sizeof(struct sockaddr_in);
	/* send to server */
	ret = sendto(sockfd, sbuf, slen, 0, servaddr, servlen);
	if (ret < 0) {
		/* we failed */
		CLIENT_PRINT("write failed, %s", strerror(errno));
		return -CLIENT_ERRNO;
	}
	CLIENT_PRINT("TX[%04d]> %s", ret, sbuf->data); /* The data sent contains 'sbuf->len', so 'ret = strlen(sbuf->data) + sizeof(sbuf->len)' */
	/* CLIENT_PRINT("send %d bytes data", ret); */

	return ret;
}

/**
 * Receive a message from the server
 *
 * @param[in] sockfd	socket file descriptor
 * @param[in] sbuf		send buff pointer
 *
 * @return On success, return the length of the sent.
 */
static int client_recv_message(int sockfd, struct common_buff *sbuf)
{
	void *rptr;
	uint32_t rlen;
	int count = 2;
	int ret;

	rptr = &sbuf->len;
	rlen = sizeof(sbuf->len);

	/* ret = recvfrom(sockfd, rlen, MAXLINE, 0, reply_addr, &len); */
	do {
		ret = read(sockfd, rptr, rlen);
		if (ret < 0) {
			CLIENT_PRINT("read failed, %s", strerror(errno));
			return -CLIENT_ERRNO;
		} else if (ret == 0) {
			CLIENT_PRINT("server closed connection");
			return 0;
		}

		if (count == 2) {
			sbuf->len = ntohl(sbuf->len);
			if (sbuf->len == 0) {
				/* data illegal */
				CLIENT_PRINT("no data read from the server");
				break;
			} else if (sbuf->len > DATA_MAX_LEN) {
				CLIENT_PRINT("data error!!!");
				return -CLIENT_ERRNO;
			}
		}

		rptr = &sbuf->data;
		rlen = sbuf->len;
	} while ((--count) > 0);

	CLIENT_PRINT("RX[%04d]> %s", rlen, sbuf->data);

	return rlen;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in servaddr;
	struct sockaddr reply_addr;
    socklen_t servlen;
	struct common_buff *buff;
	const char *ip_str;
	const char *port_str;
	uint16_t port;
	uint16_t blen;
	int sockfd;

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

	ip_str   = argv[1];
	port_str = argv[2];
	CLIENT_PRINT("addr: %s:%s", ip_str, port_str);

	servlen = sizeof(struct sockaddr_in);
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

	while (1) {
		buff->len = DATA_MAX_LEN;
		if (client_send_message(sockfd, buff, &servaddr) < 0) {
			break;
		}

		if (client_recv_message(sockfd, buff) <= 0) {
			break;
		}
	}

	if (buff) {
		free(buff);
		buff = NULL;
	}
	if (sockfd > 0) {
		close(sockfd);
		sockfd = -1;
	}

	CLIENT_PRINT("client exit...");

	return 0;
}
