#include "client_tools.h"

server svr;

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

int main(int argc, char** argv){
	// Parse arguments
	if (argc != 2){
		fprintf(stderr, "[Error] Usage: ./client [server_ip:port]\n");
		exit(1);
	}
	init_client(argv[1]);

	bool LOGIN = false, chatting = false;
	char username[64], cmd[10], filename[256], roomname[64];
	char read_buf[BUFSIZE], write_buf[BUFSIZE], buf[BUFSIZE];
	int ret = read_response(read_buf);
	if ((ret < 0) || (strncmp(read_buf, ASK_FOR_USERNAME, strlen(ASK_FOR_USERNAME)) != 0)) {
		fprintf(stderr, "[Error] read error\n");
		close(svr.fd);
		exit(1);
	}
	printf("%s", read_buf);

	while(LOGIN == false){
		fgets(write_buf, USERNAME_LIMIT, stdin);
		send(svr.fd, write_buf, strlen(write_buf), 0);
		ret = read_response(read_buf);
		if (ret < 0) {
			fprintf(stderr, "[Error] read error\n");
			close(svr.fd);
			exit(1);
		}
		printf("%s", read_buf);
		if (strncmp(read_buf, CONNECT_SUCCESS, strlen(CONNECT_SUCCESS)) == 0){
			LOGIN = true;
			write_buf[strlen(write_buf) - 1] = '\0';
			strncpy(username, write_buf, USERNAME_LIMIT);
			break;
		} else if (strncmp(read_buf, HELLO, strlen(HELLO)) == 0){
			LOGIN = true;
			write_buf[strlen(write_buf) - 1] = '\0';
			strncpy(username, write_buf, USERNAME_LIMIT);
			break;
		} else{
			continue;
		}
	}
	while (LOGIN == true){
		if (chatting == false){
			printhome();
			fprintf(stderr, "[Info] waiting for command\n");
			fgets(buf, 512, stdin);
			strncpy(write_buf, buf, 512);
			char* start = strtok(buf, " ");
			strcpy(cmd, start);
			if (strcmp(cmd, "ls\n") == 0){
				ls();
			} else if (strcmp(cmd, "get") == 0){
				start = strtok(NULL, " ");
				strcpy(filename, start);
				filename[strlen(filename) - 1] = '\0';
				get(filename);
			} else if (strcmp(cmd, "put") == 0){
				start = strtok(NULL, " ");
				strcpy(filename, start);
				filename[strlen(filename) - 1] = '\0';
				put(filename);
			} else if (strncmp(cmd, "1", 1) == 0){
				ListAllFriends();
			} else if (strncmp(cmd, "2", 1) == 0){
				AddFriend();
			} else if (strncmp(cmd, "3", 1) == 0){
				DeleteFriend();
			} else if (strncmp(cmd, "4", 1) == 0){
				CreateChatRoom(username);
			} else if (strncmp(cmd, "5", 1) == 0){
				chatting = EnterChatRoom(roomname);
			} else if (strncmp(cmd, "6", 1) == 0){
				printf("logout!\n");
				break;
			}
			else{
				printf("%s", COMMAND_NOT_FOUND);
			}
		} else {
			printchatroom(roomname);
			fprintf(stderr, "[Info] waiting for command\n");
			fgets(buf, 512, stdin);
			strncpy(write_buf, buf, 512);
			char* start = strtok(buf, " ");
			strcpy(cmd, start);
			if (strcmp(cmd, "ls\n") == 0){
				ls();
			} else if (strcmp(cmd, "get") == 0){
				start = strtok(NULL, " ");
				strcpy(filename, start);
				filename[strlen(filename) - 1] = '\0';
				get(filename);
			} else if (strcmp(cmd, "put") == 0){
				start = strtok(NULL, " ");
				strcpy(filename, start);
				filename[strlen(filename) - 1] = '\0';
				put(filename);
			} else if (strncmp(cmd, "1", 1) == 0){
				ShowHistory();
			} else if (strncmp(cmd, "2", 1) == 0){
				Refresh(roomname);
			} else if (strncmp(cmd, "3", 1) == 0){
				Text();
			} else if (strncmp(cmd, "4", 1) == 0){
				Image();
			} else if (strncmp(cmd, "5", 1) == 0){
				File();
			} else if (strncmp(cmd, "6", 1) == 0){
				chatting = !(LeaveChatRoom());
			} else{
				printf("%s", COMMAND_NOT_FOUND);
			}
		}
	}

	close(svr.fd);
	return 0;
}