#include "log.h"

struct socketops_data {
	int sockfd;
	int port;

	int domain;
	int type;
};

static void socket_set_nonblock(int sockfd)
{
	int flags;

	/* set non-blocking mode */
	flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags|O_NONBLOCK);
}

static void socket_set_cloexec(int sockfd)
{
	int flags;

	/* set non-blocking mode */
	flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags|FD_CLOEXEC);
}

static int socket_nonblock_create(struct socketops *ops, int domain, int type)
{
	struct socketops_data *data = ops->data;
	int sockfd = socket(domain, type, 0);
	if (sockfd < 0) {
		LOGERR("create socket failed, %s", strerror(errno));
		return -ERRNO;
	}

	socket_set_nonblock(sockfd);
	socket_set_cloexec(sockfd);

	data->sockfd = sockfd;
	data->domain = domain;
	data->type	 = type;

	LOGRUN("create sockte %d ok", sockfd);
	return sockfd;
}

static void socket_set_reuseaddr(struct socketops *ops, int on)
{
	struct socketops_data *data = ops->data;
	int optval = (on != 0) ? 1 : 0;
	setsockopt(data->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
}

static void socket_set_reuseport(struct socketops *ops, int on)
{
	struct socketops_data *data = ops->data;
	int optval = (on != 0) ? 1 : 0;
	setsockopt(data->sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(int));
}

static int socket_bind_addr(struct socketops *ops, int port)
{
	struct socketops_data *data = ops->data;
    struct sockaddr_in servaddr;
	int ret;

	bzero(&servaddr, sizeof(struct sockaddr_in));
    servaddr.sin_family = data->domain;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

	data->port = port;
	ret = bind(data->sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		LOGERR("bind fd %d failed, %s", data->sockfd, strerror(errno));
	}
	return ret;
}

static int socket_listen(struct socketops *ops)
{
	struct socketops_data *data = ops->data;
	int ret = listen(data->sockfd, SOMAXCONN);
	if (ret < 0) {
		LOGERR("listen fd %d failed, %s", data->sockfd, strerror(errno));
	}
	return ret;
}

struct socketops *socketops_init(void)
{
	struct socketops *ops = malloc(sizeof(struct socketops));
	struct socketops_data *data;

	if (!ops) {
		LOGERR("get socketops memory failed");
		return NULL;
	}
	memset(ops, 0x00, sizeof(struct socketops));

	data = malloc(sizeof(struct socketops_data));
	if (!data) {
		LOGERR("get socketops data memory failed");
		goto label_socketops_init;
	}

	ops->create_nonblocking = &socket_nonblocking_create;
	ops->set_reuseaddr = &socket_set_reuseaddr;
	ops->set_reuseport = &socket_set_reuseport;
	ops->bind_addr	   = &socket_bind_addr;
	ops->listen		   = &socket_listen;

	ops->data = data;

	return ops;
label_socketops_init:
	if (ops) {
		free(ops);
	}
	if (data) {
		free(data);
	}
}

void socketops_free(struct socketops *ops)
{
	if (ops) {
		struct socketops_data *data = ops->data;
		close(data->sockfd);
		free(data);
		free(ops);
	}
}
