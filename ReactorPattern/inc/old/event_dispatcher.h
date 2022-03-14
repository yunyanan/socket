#ifndef __EVENT_DISPATCHER_H__
#define  __EVENT_DISPATCHER_H__

struct event_dispatcher {
	const char *name;
	void (*init)(struct event_loop *eloop);
	int (*add)(struct event_loop *eloop, struct channel *channel);
	int (*del)(struct event_loop *eloop, struct channel *channel);
	int (*mod)(struct event_loop *eloop, struct channel *channel);

	int (*dispatch)(struct event_loop *eloop, struct timeval *time);

	void (*clear)(struct event_loop *eloop);
};

#endif	/* #ifndef __EVENT_DISPATCHER_H__ */
