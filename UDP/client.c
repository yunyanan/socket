#include <stdio.h>

#define CLIENT_ERRNO				__LINE__
#define CLIENT_PRINT(_fmt, ...)		printf("[%04d] "_fmt"\n", __LINE__, ##__VA_ARGS__);

int main(int argc, char *argv[])
{
	CLIENT_PRINT("todo");
	return 0;
}
