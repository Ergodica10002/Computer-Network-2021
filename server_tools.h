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
#include <string>

#define BUFSIZE 65536
#define USERNAME_LIMIT 64
#define BACKLOG 1024 // for listen(): number of users allowed in a queue

typedef struct {
	char hostname[512];
	unsigned short port;
	int listen_fd;
} server;

enum STATE {
	DISCONNECT, LOGOUT, LOGIN, GETTING, PUTTING
};

typedef struct {
	char hostname[512];
	int connect_fd;
	char buf[BUFSIZE];
	int buf_len;
	int ID;
	char username[USERNAME_LIMIT];
	char filename[256];
	off_t filesize, offset; // for get and put
	STATE state;
} request;

void init_server(unsigned short port);
int read_request(request* rqst);
void close_connect(request* rqst);
int check_username(char* buf);
void ls(int connect_fd);
void get(request* rqst);
void put(request* rqst);
off_t fsize(const char* filename);

extern server svr;
extern request* requestList;
extern int MAXFD;

#define ASK_FOR_USERNAME (const char*)"input your username:\n"
#define CONNECT_SUCCESS (const char*)"connect successfully\n"
#define USERNAME_EXISTED (const char*)"username is in used, please try another:\n"
#define COMMAND_NOT_FOUND (const char*)"Command not found\n"
#define COMMAND_FORMAT_ERR (const char*)"Command format error\n"
#define OK (const char*)"OK\n"
#define ERROR (const char*)"ERROR\n"