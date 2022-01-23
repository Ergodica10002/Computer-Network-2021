#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <signal.h>
#include <vector>
#include <time.h>

#define BUFSIZE 65536
#define USERNAME_LIMIT 64
#define BACKLOG 1024 // for listen(): number of users allowed in a queue
#define MAXROOMNUM 32 // Max number of chatrooms

typedef struct {
	char hostname[512];
	unsigned short port;
	int listen_fd;
} server;

enum STATE {
	UNREGISTERED, // initialized data
	DISCONNECT, // for existed disconnected user
	LOGOUT, // for new coming user
	LOGIN, // for login user
	GETTING, // for user getting file
	PUTTING, // for user puttin file
};

struct Request {
	char hostname[512];
	int connect_fd;
	char buf[BUFSIZE];
	int buf_len;
	char username[USERNAME_LIMIT];
	std::vector <struct Request*> friendList;
	int friendnum;
	char roomname[64];
	char filename[256];
	off_t filesize, offset; // for get and put
	STATE state;
};

typedef struct Request request;

enum TYPE {
	ONEBYONE, // only one assigned friend can enter
	GROUP, // all friends of users can enter
	PUBLIC // anyone can enter
};

struct Chatroom{
	char roomname[64];
	char invited_username[USERNAME_LIMIT];
	std::vector <struct Request*> users;
	int usernum;
	struct Request* admin;
	TYPE type;
};

typedef struct Chatroom chatroom;

void init_request(request* rqst);
void init_server(unsigned short port);
int find_empty_fd(void);
int read_request(request* rqst);
void close_connect(request* rqst);
int check_username(char* buf);
void ls(int connect_fd);
bool WriteHistory(char* roomname, char* sender, char* text, int puttype);
void get(request* rqst);
void put(request* rqst);
off_t fsize(const char* filename);
void ListAllFriends(request* rqst);
void AddFriend(request* rqst);
void DeleteFriend(request* rqst);
bool isFriend(char* user1, char* user2);
void CreateChatRoom(request* rqst);
void ListChatRoom(request* rqst);
void EnterChatRoom(request* rqst);
void Text(request* rqst);
void Leave(request* rqst);

extern server svr;
extern request* requestList;
extern chatroom* roomList;
extern int MAXFD;

#define ASK_FOR_USERNAME (const char*)"input your username:\n"
#define CONNECT_SUCCESS (const char*)"connect successfully\n"
#define USERNAME_EXISTED (const char*)"username is in used, please try another:\n"
#define COMMAND_NOT_FOUND (const char*)"Command not found\n"
#define COMMAND_FORMAT_ERR (const char*)"Command format error\n"
#define USERNAME_TOOLONG (const char*)"username is too long, please try another:\n"
#define OK (const char*)"OK\n"
#define ERROR (const char*)"ERROR\n"
#define HISTORY (char*)"history"