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
#include <time.h>
#include <string>
#include <signal.h>
#include <vector>

#define BUFSIZE 65536
#define USERNAME_LIMIT 64
#define BACKLOG 1024 // for listen(): number of users allowed in a queue
#define MAXROOMNUM 32 // Max number of chatrooms

typedef struct {
	char ip_addr[512];
	unsigned short port;
	int fd;
} server;

typedef struct {
	char hostname[512];
	unsigned short port;
	int listen_fd;
} WebServer;

typedef struct Request {
	char hostname[512];
	int connect_fd;
	char buf[BUFSIZE];
	int buf_len;
	char filename[256];
	off_t filesize, offset; // for get and put
} request;

void init_client(char* addr);
void init_webserver(unsigned short port);
int read_response(char* read_buf);
int read_request(request* rqst);
void close_connect(request* rqst);
void ls(void);
void get(char* filename);
void put(char* filename);
off_t fsize(const char* filename);
void printhome(void);
void printchatroom(char* roomname);
void ListAllFriends(char* html_filename);
void AddFriend(char* html_filename);
void DeleteFriend(char* html_filename);
void CreateChatRoom(char* username, char* html_filename);
bool EnterChatRoom(char* roomname, char* html_filename);
void ShowHistory(char* roomname, char* html_filename);
int getHistory(char time_list[256][32], char sender_list[256][USERNAME_LIMIT], char text_list[256][256], char image_list[256][64], char file_list[256][64]);
void Refresh(char* roomname, char* html_filename);
void Text(char* roomname, char* html_filename);
void Image(char* roomname, char* html_filename);
void File(char* roomname, char* html_filename);
void Download(char* roomname, char* html_filename);
bool LeaveChatRoom(char* roomname, char* html_filename);

void parse_http_request(char* http_request, int* http_type, char* filename, char* http_data);
void send_http_file(int browser_fd, char* filename, const char* content_type_str, int close_connection);
bool Add_info_to_Home(const char* Home_filename, char* write_buf, char* html_filename);
bool Add_info_to_Room(const char* Room_filename, char* roomname, char* write_buf, char* html_filename);

extern server svr;
extern WebServer websvr;
extern request browser;
extern int MAXFD;

#define ASK_FOR_USERNAME (const char*)"Enter your username:\n"
#define CONNECT_SUCCESS (const char*)"connect successfully\n"
#define USERNAME_EXISTED (const char*)"username is in used, please try another:\n"
#define USERNAME_TOOLONG (const char*)"username is too long, please try another:\n"
#define HELLO (const char*)"Hello"
#define COMMAND_NOT_FOUND (const char*)"Command not found\n"
#define COMMAND_FORMAT_ERR (const char*)"Command format error\n"
#define OK (const char*)"OK\n"
#define ERROR (const char*)"ERROR\n"
#define HISTORY (char*)"history"

#define http_header_OK (const char*)"HTTP/1.1 200 OK"
#define http_header_NOTFOUND (const char*)"HTTP/1.1 404 Not Found"
#define http_connection_close (const char*)"Connection: close"
#define http_content_length (const char*)"Content-Length:"
#define http_content_type_html (const char*)"Content-Type: text/html"
#define http_content_type_img (const char*)"Content-Type: image/"
#define http_blank_str (const char*)" "
#define html_Login_page (const char*)"../sources/Login.html"
#define html_Home_page (const char*)"../sources/Home.html"
#define html_Home_AddFriend_page (const char*)"../sources/Home_AddFriend.html"
#define html_Home_DeleteFriend_page (const char*)"../sources/Home_DeleteFriend.html"
#define html_Home_CreateChatRoom_page (const char*)"../sources/Home_CreateChatRoom.html"
#define html_Home_EnterChatRoom_page (const char*)"../sources/Home_EnterChatRoom.html"

#define html_Room_page (const char*)"../../sources/Room.html"
#define html_Room_Download_page (const char*)"../../sources/Room_Download.html"
#define html_Room_Text_page (const char*)"../../sources/Room_Text.html"
#define html_Room_File_page (const char*)"../../sources/Room_File.html"
#define html_Room_added_page (const char*)"../../sources/Room_added.html"

