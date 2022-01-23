#include "client_webtools.h"

server svr;
WebServer websvr;
request browser;
int MAXFD;
bool running = true;

using namespace std;

/* 
Support Command:
	ls: List all files in server directory.
	get filename: download file filename.
	put filename: upload file filename.
	1: List all friends.
	2: Add a friend.
	3: Delete a friend.
	4: Choose a chat room
*/

void serve_home(char* buf, bool* chatting_ptr, char* username, char* roomname, char* html_filename);
void serve_chatting(char* buf, bool* chatting_ptr, char* roomname, char* html_filename);

int main(int argc, char** argv){
	// Parse arguments
	if (argc != 3){
		fprintf(stderr, "[Error] Usage: ./client [server ip:port] [local port]\n");
		exit(1);
	}
	bool LOGIN = false, chatting = false;
	char username[64], roomname[64], filename[64], html_filename[64];
	char read_buf[BUFSIZE], write_buf[BUFSIZE], http_data[BUFSIZE];
	int http_type; // 0: GET; 1:POST
	int readnum, offset;
	off_t filesize;
	const char* content_type_str;
	init_client(argv[1]);

	int fdnum; // for select() to return number of new requests
	int client_fd; // for a new connection
	int file_fd; // for file operation
	fd_set readset; // for select() to know which fd to listen
	struct timeval timeout; // for select() to wait
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
	unsigned short web_port = (unsigned short) atoi(argv[2]);
	MAXFD = getdtablesize();
	init_webserver(web_port);
	printf("[Info] web server running at %s:%u\n", websvr.hostname, websvr.port);

	int ret = read_response(read_buf);
	printf("%s", read_buf);

	while(running) {
		FD_ZERO(&readset);
		FD_SET(websvr.listen_fd, &readset); // server listening for new-coming clients
		FD_SET(STDIN_FILENO, &readset);	
		if (browser.connect_fd != -1){
			FD_SET(browser.connect_fd, &readset);
		}

		fdnum = select(MAXFD, &readset, NULL, NULL, &timeout); // find whether a new request comes

		// when new request log in
		if (browser.connect_fd < 0 && FD_ISSET(websvr.listen_fd, &readset)){
			struct sockaddr_in cliaddr;  // used by accept()
    		int clilen = sizeof(cliaddr);
    		client_fd = accept(websvr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
    		if (client_fd < 0){
    			if (errno == EINTR || errno == EAGAIN) continue;  // try again
                if (errno == ENFILE) {
                    fprintf(stderr, "[Error] out of fd limit ... (maxconn %d)\n", MAXFD);
                    continue;
                }
                fprintf(stderr, "[Error] accept error\n");
    		}
    		browser.connect_fd = client_fd;
    		strcpy(browser.hostname, inet_ntoa(cliaddr.sin_addr));
    		fprintf(stderr, "\n[Info] Getting a new browser request... fd %d from %s\n", client_fd, browser.hostname);
		}

		if (FD_ISSET(STDIN_FILENO, &readset)){
			if (LOGIN == false){
				fgets(write_buf, USERNAME_LIMIT, stdin);
				send(svr.fd, write_buf, strlen(write_buf), 0);
				ret = read_response(read_buf);
				if (ret < 0) {
					fprintf(stderr, "[Error] read server error\n");
					close(svr.fd);
					exit(1);
				}
				printf("%s", read_buf);
				if (strncmp(read_buf, CONNECT_SUCCESS, strlen(CONNECT_SUCCESS)) == 0){
					LOGIN = true;
					write_buf[strlen(write_buf) - 1] = '\0';
					strncpy(username, write_buf, USERNAME_LIMIT);
					printhome();
				} else if (strncmp(read_buf, HELLO, strlen(HELLO)) == 0){
					LOGIN = true;
					write_buf[strlen(write_buf) - 1] = '\0';
					strncpy(username, write_buf, USERNAME_LIMIT);
					printhome();
				}
			} else {
				fgets(read_buf, 512, stdin);
				if (chatting == false){
					serve_home(read_buf, &chatting, username, roomname, html_filename);
				} else {
					serve_chatting(read_buf, &chatting, roomname, html_filename);
				}
				if (running == false) {
					break;
				}
				if (chatting == false){
					printhome();
				} else {
					printchatroom(roomname);	
				}
			}
		}

		if (browser.connect_fd >= 0 && FD_ISSET(browser.connect_fd, &readset)){
			ret = read_request(&browser);
			if (ret <= 0){
				close_connect(&browser);
				continue;
			}
			parse_http_request(browser.buf, &http_type, filename, http_data);

			if (LOGIN == false) {
				if (http_type == 1 && strstr(http_data, "username") != NULL){
					send(svr.fd, http_data + 9, strlen(http_data + 9), 0);
					ret = read_response(read_buf);
					if (ret < 0) {
						fprintf(stderr, "[Error] read server error\n");
						close(svr.fd);
						exit(1);
					}
					printf("%s", read_buf);
					if (strncmp(read_buf, CONNECT_SUCCESS, strlen(CONNECT_SUCCESS)) == 0 \
						|| strncmp(read_buf, HELLO, strlen(HELLO)) == 0){
						LOGIN = true;
						strncpy(username, http_data + 9, USERNAME_LIMIT);
						printf("Hello %s\n", username);
						printhome();
						strcpy(filename, html_Home_page);
						send_http_file(browser.connect_fd, filename, http_content_type_html, 1);
					}
				} else {
					if (strlen(filename) == 0) {
						strcpy(filename, html_Login_page);
						content_type_str = http_content_type_html;
					} else {
						if (strstr(filename, "jpeg") != NULL || strstr(filename, "ico") != NULL){
							content_type_str = http_content_type_img;
						} else {
							content_type_str = http_blank_str;
						}
					}
					send_http_file(browser.connect_fd, filename, content_type_str, 1);
				}
			} else {
				if (chatting == false) {
					if (http_type == 1 && strstr(http_data, "command") != NULL){
						char* command = http_data + 8;
						serve_home(command, &chatting, username, roomname, html_filename);
						if (chatting == false){
							printhome();
						} else {
							printchatroom(roomname);	
						}
						strcpy(filename, html_filename);
						content_type_str = http_content_type_html;
					} else {
						if (strlen(filename) == 0) {
							strcpy(filename, html_Home_page);
							content_type_str = http_content_type_html;
						} else {
							if (strstr(filename, "jpeg") != NULL || strstr(filename, "ico") != NULL){
								content_type_str = http_content_type_img;
							} else {
								content_type_str = http_blank_str;
							}
						}
					}
				} else {
					if (http_type == 1 && strstr(http_data, "command") != NULL) {
						char* command = http_data + 8;
						serve_chatting(command, &chatting, roomname, html_filename);
						if (chatting == false){
							printhome();
						} else {
							printchatroom(roomname);
						}
						strcpy(filename, html_filename);
						content_type_str = http_content_type_html;
					} else {
						if (strlen(filename) == 0) {
							strcpy(filename, html_Room_page);
							content_type_str = http_content_type_html;
						} else {
							if (strstr(filename, "jpeg") != NULL || strstr(filename, "ico") != NULL){
								content_type_str = http_content_type_img;
							} else {
								content_type_str = http_blank_str;
							}
						}
					}
				}
				send_http_file(browser.connect_fd, filename, content_type_str, 1);
			}
		}
	}

	// serve_user();
	close(svr.fd);
	return 0;
}

void serve_home(char* buf, bool* chatting_ptr, char* username, char* roomname, char* html_filename){
	char cmd[32];
	strncpy(cmd, buf, 32);
	if (strncmp(cmd, "1", 1) == 0 || strncmp(cmd, "ListAllFriends", 14) == 0){
		ListAllFriends(html_filename);
	} else if (strncmp(cmd, "2", 1) == 0 || strncmp(cmd, "AddFriend", 9) == 0){
		AddFriend(html_filename);
	} else if (strncmp(cmd, "3", 1) == 0 || strncmp(cmd, "DeleteFriend", 12) == 0){
		DeleteFriend(html_filename);
	} else if (strncmp(cmd, "4", 1) == 0 || strncmp(cmd, "CreateChatRoom", 14) == 0){
		CreateChatRoom(username, html_filename);
	} else if (strncmp(cmd, "5", 1) == 0 || strncmp(cmd, "EnterChatRoom", 13) == 0){
		*chatting_ptr = EnterChatRoom(roomname, html_filename);
	} else if (strncmp(cmd, "6", 1) == 0 || strncmp(cmd, "Logout", 6) == 0){
		printf("Logout!\n");
		running = false;
	} else{
		printf("%s", COMMAND_NOT_FOUND);
	}
	return;
}

void serve_chatting(char* buf, bool* chatting_ptr, char* roomname, char* html_filename){
	char filename[128];
	char cmd[32];
	strncpy(cmd, buf, 32);
	if (strncmp(cmd, "1", 1) == 0 || strncmp(cmd, "ShowHistory", 11) == 0){
		ShowHistory(roomname, filename);
	} else if (strncmp(cmd, "2", 1) == 0 || strncmp(cmd, "Text", 4) == 0){
		Text(roomname, html_filename);
	} else if (strncmp(cmd, "3", 1) == 0 || strncmp(cmd, "Image", 5) == 0){
		Image(roomname, html_filename);
	} else if (strncmp(cmd, "4", 1) == 0 || strncmp(cmd, "File", 4) == 0){
		File(roomname, html_filename);
	} else if (strncmp(cmd, "5", 1) == 0 || strncmp(cmd, "Download", 8) == 0){
		Download(roomname, html_filename);
	} else if (strncmp(cmd, "6", 1) == 0 || strncmp(cmd, "Leave", 5) == 0){
		*chatting_ptr = !(LeaveChatRoom(roomname, html_filename));
	} else{
		printf("%s", COMMAND_NOT_FOUND);
	}
	return;
}
