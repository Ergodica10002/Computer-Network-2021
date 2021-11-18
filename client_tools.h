#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>

#define BUFSIZE 65536
#define USERNAME_LIMIT 64

typedef struct {
	char ip_addr[512];
	unsigned short port;
	int fd;
} server;

void init_client(char* addr);
int read_response(char* read_buf);
void ls(void);
void get(char* filename);
void put(char* filename);
off_t fsize(const char* filename);

extern server svr;

#define ASK_FOR_USERNAME (const char*)"input your username:\n"
#define CONNECT_SUCCESS (const char*)"connect successfully\n"
#define USERNAME_EXISTED (const char*)"username is in used, please try another:\n"
#define COMMAND_NOT_FOUND (const char*)"Command not found\n"
#define COMMAND_FORMAT_ERR (const char*)"Command format error\n"
#define OK (const char*)"OK\n"
#define ERROR (const char*)"ERROR\n"