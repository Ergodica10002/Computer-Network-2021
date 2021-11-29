#include "server_tools.h"

void init_request(){
	for (int i = 0; i < MAXFD; i++){
		requestList[i].connect_fd = -1;
		requestList[i].buf_len = 0;
		requestList[i].ID = 0;
		requestList[i].state = STATE::DISCONNECT;
	}
	return;
}
void init_server(unsigned short port){
	struct sockaddr_in servaddr;

	gethostname(svr.hostname, sizeof(svr.hostname));
	svr.port = port;
	svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		fprintf(stderr, "[Error] bind error\n");
		exit(1);
	}
	if (listen(svr.listen_fd, BACKLOG) < 0){
		fprintf(stderr, "[Error] listen error\n");
		exit(1);
	}

	requestList = new request[MAXFD];
	if (requestList == NULL){
		fprintf(stderr, "[Error] Memory allocate error\n");
		exit(1);
	}
	init_request();
	requestList[svr.listen_fd].connect_fd = svr.listen_fd;
	strcpy(requestList[svr.listen_fd].hostname, svr.hostname);

	if (mkdir("server_dir", S_IRWXU) < 0){
		if (errno != EEXIST){
			fprintf(stderr, "[Error] mkdir error\n");
			exit(1);
		}
	}
	chdir("server_dir");

	return;
}

int read_request(request* rqst){
	/* 
		Empty rqst->buf first and read socket contents into rqst->buf
	*/
	memset(rqst->buf, '\0', BUFSIZE);
	int ret = recv(rqst->connect_fd, rqst->buf, BUFSIZE, 0);
	rqst->buf_len = ret;
	return ret;
}

void close_connect(request* rqst){
	fprintf(stderr, "[Info] Close connection %d\n", rqst->connect_fd);
	close(rqst->connect_fd);
	rqst->connect_fd = -1;
	rqst->buf_len = 0;
	rqst->ID = 0;
	rqst->state = STATE::DISCONNECT;
	return;
}

int check_username(char* buf){
	/*
		If username is not registered, return 0
		If username is registered, return < -1
		If username is out of length, return -2
	*/ 
	int len = strlen(buf);
	if (len > USERNAME_LIMIT) {
		fprintf(stderr, "[Error] Exceed username length limit\n");
		return -2;
	}
	buf[len - 1] = '\0';
	for (int i = 0; i < MAXFD; i++){
		if (requestList[i].state != STATE::DISCONNECT && requestList[i].state != STATE::LOGOUT){
			if (strncmp(buf, requestList[i].username, USERNAME_LIMIT) == 0){
				return -1;
			} 
		}
	}
	return 0;
}

void ls(int connect_fd){
	/*
		After receive the command, find and respond all file names in one block
	*/
	fprintf(stderr, "[Info] receive request ls from fd %d\n", connect_fd);
	DIR* dp = opendir(".");
	struct dirent *dirp;
	if (dp == NULL){
		fprintf(stderr, "[Error] opendir error\n");
		send(connect_fd, ERROR, strlen(ERROR), 0);
		return;
	}
	char buf[BUFSIZE] = {'\0'};
	off_t offset = 0;
	while ((dirp = readdir(dp)) != NULL) {
		if (strncmp(dirp->d_name, ".", 1) == 0){
			continue;
		}
		sprintf(buf + offset, "%s\n", dirp->d_name);
		offset = offset + strlen(dirp->d_name) + 1;
	}
	if (strlen(buf) > 0){
		send(connect_fd, buf, strlen(buf), 0);	
	} else {
		send(connect_fd, ERROR, strlen(ERROR), 0);
	}
	closedir(dp);
	return;
}

off_t fsize(const char* filename) {
	struct stat st;
	if (stat(filename, &st) == 0) {
		return st.st_size;
	}
	fprintf(stderr, "[Error] file size error: %s\n", filename);
	return -1;
}

void get(request* rqst){
	/* 
		At the first request command, respond the file size or error message if not exist
		Server delivers file contents blocks by blocks
		Client responds OK for each block received
	*/
	char buf[BUFSIZE];
	int file_fd;
	int readnum, writenum;

	if (rqst->state == STATE::LOGIN) {
		fprintf(stderr, "[Info] receive request get from fd %d\n", rqst->connect_fd);
		// respond file size or error message
		strcpy(rqst->filename, rqst->buf + 4);
		rqst->filename[strlen(rqst->filename) - 1] = '\0';
		file_fd = open(rqst->filename, O_RDONLY);
		if (file_fd < 0){
			if (errno == ENOENT){
				fprintf(stderr, "[Info] request file %s not exist\n", rqst->filename);
				send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
			} else {
				send(rqst->connect_fd, COMMAND_FORMAT_ERR, strlen(COMMAND_FORMAT_ERR), 0);
			}
			return;
		}
		rqst->filesize = fsize(rqst->filename);
		rqst->offset = 0;
		rqst->state = STATE::GETTING;
		fprintf(stderr, "[Info] filename:%s filesize:%ld\n", rqst->filename, rqst->filesize);

		sprintf(buf, "%ld", rqst->filesize);
		send(rqst->connect_fd, buf, strlen(buf), 0); // send file size to client

		close(file_fd);
	} else {
		if (strncmp(rqst->buf, ERROR, strlen(ERROR)) == 0) {
			fprintf(stderr, "[Error] client error\n");
			send(rqst->connect_fd, OK, strlen(OK), 0);
			rqst->state = STATE::LOGIN;
			return;
		}
		file_fd = open(rqst->filename, O_RDONLY);
		lseek(file_fd, rqst->offset, SEEK_SET);
		readnum = read(file_fd, buf, sizeof(buf));
		writenum = send(rqst->connect_fd, buf, readnum, 0);

		rqst->offset += writenum;
		if (rqst->offset >= rqst->filesize){
			rqst->state = STATE::LOGIN;
			fprintf(stderr, "[Info] get %s successfully\n", rqst->filename);
			fprintf(stderr, "[Info] total write: %ld bytes\n", rqst->offset);
		}

		close(file_fd);
	}

	return;
}

void put(request* rqst){
	/* 
		At the first request command, receive the command type and file size
		Client delivers file contents blocks by blocks
		Server responds OK for each block received
	*/
	int file_fd;
	int writenum;

	if (rqst->state == STATE::LOGIN) {
		fprintf(stderr, "[Info] receive request put from fd %d\n", rqst->connect_fd);
		// get command and file size
		char* start = strtok(rqst->buf + 4, " ");
		strcpy(rqst->filename, start);
		start = strtok(NULL, " ");
		rqst->filesize = atoi(start);
		if (rqst->filesize < 0) {
			send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
			return;
		}
		file_fd = open(rqst->filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (file_fd < 0){
			send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
			return;
		}
		rqst->offset = 0;
		rqst->state = STATE::PUTTING;
		fprintf(stderr, "[Info] filename:%s filesize:%ld\n", rqst->filename, rqst->filesize);

		send(rqst->connect_fd, OK, strlen(OK), 0);

		close(file_fd);
	} else {
		send(rqst->connect_fd, OK, strlen(OK), 0);
		file_fd = open(rqst->filename, O_WRONLY);
		lseek(file_fd, rqst->offset, SEEK_SET);
		writenum = write(file_fd, rqst->buf, rqst->buf_len);
		rqst->offset += writenum;
		if (rqst->offset >= rqst->filesize){
			rqst->state = STATE::LOGIN;
			fprintf(stderr, "[Info] put %s successfully\n", rqst->filename);
		}
		close(file_fd);
	}

	return;
}