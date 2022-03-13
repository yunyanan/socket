#ifndef __EVENT_LOOP_H__
#define __EVENT_LOOP_H__

#define EVENT_ERRNO						__LINE__

struct event_loop {
	const char *name;
	int quit;

	pthread_t pid;

	const struct event_dispatcher *dispatcher;
};

struct event_loop *event_loop_init(const char *name);

#endif	/* #ifndef __EVENT_LOOP_H__ */
