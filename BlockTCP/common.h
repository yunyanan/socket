#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

#define DATA_MAX_LEN	1024
struct common_buff {
	uint32_t len;
	uint8_t data[0];
};

#endif	/* #ifndef __COMMON_H__ */
