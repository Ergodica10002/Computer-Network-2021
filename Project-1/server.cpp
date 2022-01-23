#include "server_tools.h"

server svr;
request* requestList = NULL;
int MAXFD;

using namespace std;

int main(int argc, char** argv){
	// Parse arguments
	if (argc != 2){
		fprintf(stderr, "[Error] Usage: ./server [port]\n");
		exit(1);
	}

	unsigned short server_port = (unsigned short) atoi(argv[1]);
	MAXFD = getdtablesize();
	init_server(server_port);

	int fdnum; // for select() to return number of new requests
	int client_fd; // for a new connection
	int file_fd; // for file operation
	char buf[512];
	int buf_len;
	fd_set readset; // for select() to know which fd to listen
	struct timeval timeout; // for select() to wait
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

	while(1) {
		FD_ZERO(&readset);
		FD_SET(svr.listen_fd, &readset); // server listening for new-coming clients
		for (int i = 0; i < MAXFD; i++){
			if (requestList[i].connect_fd != -1){
				FD_SET(requestList[i].connect_fd, &readset); // server listening for connected clients
			}
		}

		fdnum = select(MAXFD, &readset, NULL, NULL, &timeout); // find whether a new request comes

		// when new request log in
		if (FD_ISSET(svr.listen_fd, &readset)){
			struct sockaddr_in cliaddr;  // used by accept()
    		int clilen = sizeof(cliaddr);
    		client_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
    		if (client_fd < 0){
    			if (errno == EINTR || errno == EAGAIN) continue;  // try again
                if (errno == ENFILE) {
                    fprintf(stderr, "[Error] out of fd limit ... (maxconn %d)\n", MAXFD);
                    continue;
                }
                fprintf(stderr, "[Error] accept error\n");
    		}

    		requestList[client_fd].connect_fd = client_fd;
    		strcpy(requestList[client_fd].hostname, inet_ntoa(cliaddr.sin_addr));
    		requestList[client_fd].state = STATE::LOGOUT;
    		fprintf(stderr, "[Info] Getting a new request... fd %d from %s\n", client_fd, requestList[client_fd].hostname);
    		write(requestList[client_fd].connect_fd, ASK_FOR_USERNAME, strlen(ASK_FOR_USERNAME));
    		continue;
		}

		// handle requests from connected clients
		for (int i = 0; i < MAXFD; i++){
			if (requestList[i].connect_fd >= 0 && FD_ISSET(requestList[i].connect_fd, &readset)){
				int ret = read_request(&requestList[i]);
				if (ret < 0){
					fprintf(stderr, "[Error] read request error\n");
					continue;
				} else if (ret == 0){
					close_connect(&requestList[i]);
					continue;
				}

				switch(requestList[i].state) {
					case STATE::LOGOUT:
						if (check_username(requestList[i].buf) >= 0) {
							requestList[i].state = STATE::LOGIN;
							strcpy(requestList[i].username, requestList[i].buf);
							send(requestList[i].connect_fd, CONNECT_SUCCESS, strlen(CONNECT_SUCCESS), 0);
							fprintf(stderr, "[Info] receive login %s from fd %d\n", requestList[i].username, requestList[i].connect_fd);
						} else{
							send(requestList[i].connect_fd, USERNAME_EXISTED, strlen(USERNAME_EXISTED), 0);
						}
						break;
					case STATE::LOGIN:
						if (strncmp(requestList[i].buf, "ls", 2) == 0) {
							ls(requestList[i].connect_fd);
						} else if (strncmp(requestList[i].buf, "get", 3) == 0) {
							get(&requestList[i]);
						} else if (strncmp(requestList[i].buf, "put", 3) == 0) {
							put(&requestList[i]);
						} else {
							send(requestList[i].connect_fd, COMMAND_NOT_FOUND, strlen(COMMAND_NOT_FOUND), 0);
						}
						break;
					case STATE::GETTING:
						get(&requestList[i]);
						break;
					case STATE::PUTTING:
						put(&requestList[i]);
						break;
					default:
						fprintf(stderr, "[Error] Disconnect error\n");
						close_connect(&requestList[i]);
				}
			}
		}
	}

	delete [] requestList;
	return 0;
}