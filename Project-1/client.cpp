#include "client_tools.h"

server svr;

using namespace std;

int main(int argc, char** argv){
	// Parse arguments
	if (argc != 2){
		fprintf(stderr, "[Error] Usage: ./client [ip:port]\n");
		exit(1);
	}
	init_client(argv[1]);

	bool LOGIN = false;
	char username[64], cmd[5], filename[256];
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
		if (strncmp(read_buf, USERNAME_EXISTED, strlen(USERNAME_EXISTED)) == 0){
			continue;
		} else if (strncmp(read_buf, CONNECT_SUCCESS, strlen(CONNECT_SUCCESS)) == 0){
			LOGIN = true;
			write_buf[strlen(write_buf) - 1] = '\0';
			strncpy(username, write_buf, USERNAME_LIMIT);
			break;
		}
	}
	while (LOGIN == true){
		fprintf(stderr, "[Info] waiting for command\n");
		fgets(buf, 512, stdin);
		strncpy(write_buf, buf, 512);
		char* start = strtok(buf, " ");
		strcpy(cmd, start);
		if (strcmp(cmd, "ls\n") == 0) {
			ls();
			continue;
		} else if (strcmp(cmd, "get") == 0) {
			start = strtok(NULL, " ");
			strcpy(filename, start);
			filename[strlen(filename) - 1] = '\0';
			get(filename);
		} else if (strcmp(cmd, "put") == 0) {
			start = strtok(NULL, " ");
			strcpy(filename, start);
			filename[strlen(filename) - 1] = '\0';
			put(filename);
		} else {
			printf("%s", COMMAND_NOT_FOUND);
		}
	}

	close(svr.fd);
	return 0;
}