#ifndef __EVENT_DISPATCHER_H__
#define __EVENT_DISPATCHER_H__

struct event_dispatcher {
	void (*init)();
	int (*add)();
	int (*del)();
	int (*update)();
	int (*dispatch)();
};

#endif	/* #ifndef __EVENT_DISPATCHER_H__ */
