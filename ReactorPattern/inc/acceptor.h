#ifndef __ACCEPTOR_H__
#define __ACCEPTOR_H__

#include "socket.h"

struct acceptor {
	struct socketops *socketops;
};
struct accetpor *acceptor_init(int port);

#endif	/* #ifndef __ACCEPTOR_H__ */
