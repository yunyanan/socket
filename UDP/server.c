#include <stdio.h>

#define SERVER_ERRNO				__LINE__
#define SERVER_PRINT(_fmt, ...)		printf("[%04d] "_fmt"\n", __LINE__, ##__VA_ARGS__);

int main(int argc, char *argv[])
{
	SERVER_PRINT("todo");
	return 0;
}
