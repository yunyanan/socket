#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

#define DATA_MAX_LEN	1024
struct common_buff {
	uint8_t data[DATA_MAX_LEN];
};

#endif	/* #ifndef __COMMON_H__ */
