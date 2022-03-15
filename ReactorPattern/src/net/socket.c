#include "log.h"

struct socketops_data {
	int sockfd;
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

static int socket_nonblock_create(int domain, int type)
{
	int sockfd = socket(domain, type, 0);
	if (sockfd < 0) {
		LOGERR("create socket failed, %s", strerror(errno));
		return -ERRNO;
	}

	socket_set_nonblock(sockfd);
	socket_set_cloexec(sockfd);

	LOGRUN("create sockte %d ok", sockfd);
	return sockfd;
}

static void socket_set_reuseaddr(int on)
{
	int optval = (on != 0) ? 1 : 0;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
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
		free(ops);
	}
}
