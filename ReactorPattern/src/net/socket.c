#include "log.h"

void socket_set_nonblock(int sockfd)
{
	int flags;

	/* set non-blocking mode */
	flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags|O_NONBLOCK);
}

void socket_set_cloexec(int sockfd)
{
	int flags;

	/* set non-blocking mode */
	flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags|FD_CLOEXEC);
}

int socket_nonblock_create(int domain, int type)
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
