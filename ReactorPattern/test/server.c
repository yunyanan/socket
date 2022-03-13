#include <stdio.h>

#include "log.h"
#include "event_loop.h"

#define SERVER_ERRNO				__LINE__

int main(int argc, char **argv)
{
	LOG(LOG_LVL_ERROR, "Log message");
	LOGERR("Error message %d",	 LOG_LVL_ERROR);
	LOGWAN("Warning message %d", LOG_LVL_WARNG);
	LOGDBG("Debug message %d",	 LOG_LVL_DEBUG);
	LOGRUN("Running message %d", LOG_LVL_RUNIG);


	struct event_loop *eloop = event_loop_init("test");
	if (!eloop) {
		LOGERR("event loop init failed");
		return -SERVER_ERRNO;
	}

	LOGRUN("loop name: %s", eloop->name);

	event_loop_run(eloop);

	return 0;
}
