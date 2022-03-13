#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "event_loop.h"

struct event_loop *event_loop_init(const char *name)
{
	struct event_loop *eloop;

	eloop = malloc(sizeof(struct event_loop));
	if (!eloop) {
		LOGERR("get event loop memory failed");
		return NULL;
	}
	memset(eloop, 0x00, sizeof(struct event_loop));
	eloop->name = name;
	eloop->quit = 0;
	eloop->pid	= pthread_self();

	LOGRUN("event loop init ok");

	return eloop;
}

int event_loop_run(struct event_loop *eloop)
{
	if (!eloop) {
		LOGERR("loop run failed");
		return -EVENT_ERRNO;
	}

	LOGDBG("%s event loop run", eloop->name);

	while (!eloop->quit) {
		break;
	}

	LOGDBG("%s event loop quit", eloop->name);

	return 0;
}
