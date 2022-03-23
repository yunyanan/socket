#include "log.h"
#include "acceptor.h"

struct accetpor *acceptor_init(int port)
{
	struct acceptor *acceptor = malloc(sizeof(struct acceptor));
	struct socketops *socketops;
	int sockfd;

	if (!acceptor) {
		LOGERR("get acceptor memory failed");
		return NULL;
	}
	memset(acceptor, 0x00, sizeof(struct acceptor));

	socketops = socketops_init();
	if (!socketops) {
		LOGERR("socket ops init failed");
		goto label_acceptor_init;
	}

	sockfd = socketops->create_nonblocking(socketops, AF_INET, SOCK_STREAM);
	if (sockfd < 0) {
		LOGERR("acceptor create socket failed");
		goto label_acceptor_init;
	}

	socketops->set_reuseaddr(socketops, 1);
	socketops->set_reuseport(socketops, 1);
	if (socketops->bind_addr(socketops, port) < 0) {
		LOGERR("acceptor bind socket failed");
		goto label_acceptor_init;
	}

	if (socketops->listen(socketops) < 0) {
		LOGERR("acceptor listen socket failed");
		goto label_acceptor_init;
	}

	acceptor->socketops = socketops;

	LOGRUN("acceptor init ok");
	return acceptor;
label_acceptor_init:
	if (acceptor) {
		free(acceptor);
	}
	if (socketops) {
		socketops_free(socketops);
	}
	return NULL;
}

void acceptor_destroy(struct acceptor *acceptor)
{
	if (acceptor) {
		if (acceptor->socketops) {
			socketops_free(acceptor->socketops);
		}
		free(acceptor);
	}
}
