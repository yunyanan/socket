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

	sockfd = socketops->create_nonblocking(AF_INET, SOCK_STREAM);
	if (sockfd < 0) {
		LOGERR("acceptor create socket failed");
		free(acceptor);
		goto label_acceptor_init;
	}

	acceptor->sockfd = sockfd;
	acceptor->port	 = port;

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
