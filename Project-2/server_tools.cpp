#include "server_tools.h"

void init_request(request* rqst){
	if (rqst == NULL) {
		for (int i = 0; i < MAXFD; i++){
			requestList[i].connect_fd = -1;
			requestList[i].buf_len = 0;
			requestList[i].state = STATE::UNREGISTERED;
			requestList[i].friendnum = 0;
			memset(requestList[i].roomname, '\0', 64);
		}
	} else {
		rqst->connect_fd = -1;
		rqst->state = STATE::UNREGISTERED;
	}
	return;
}

void init_chatroom(chatroom* room){
	for (int i = 0; i < MAXROOMNUM; i++){
		memset(roomList[i].roomname, '\0', 64);
		roomList[i].usernum = 0;
	}
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
	init_request(NULL);

	roomList = new chatroom[MAXROOMNUM];
	if (roomList == NULL){
		fprintf(stderr, "[Error] Memory allocate error\n");
		exit(1);
	}
	init_chatroom(NULL);

	if (mkdir("server_dir", S_IRWXU) < 0){
		if (errno != EEXIST){
			fprintf(stderr, "[Error] mkdir error\n");
			exit(1);
		}
	}
	chdir("server_dir");

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		fprintf(stderr, "[Error] Catch SIGPIPE error\n");
		exit(1);
	}

	return;
}

int find_empty_fd() {
	for (int i = 0; i < MAXFD; i++){
		if (requestList[i].state == STATE::UNREGISTERED){
			return i;
		}
	}
	return -1;
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
	rqst->state = STATE::DISCONNECT;
	memset(rqst->roomname, '\0', 64);
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
		fprintf(stderr, "Exceed user name limit!\n");
		return -3;
	}
	if (buf[len - 1] == '\n'){
		buf[len - 1] = '\0';
	} 
	for (int i = 0; i < MAXFD; i++){
		if (strncmp(buf, requestList[i].username, USERNAME_LIMIT) == 0){
			if (requestList[i].state == STATE::DISCONNECT){
				return i;
			} else {
				return -2;
			}
		}
	}
	return -1;
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
	char buf[BUFSIZE];
	memset(buf, '\0', BUFSIZE);
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

bool WriteHistory(char* roomname, char* sender, char* text, int puttype){
	char history_filename[80], write_str[BUFSIZE];
	sprintf(history_filename, "%s/%s", roomname, HISTORY);
	int history_fd = open(history_filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (history_fd < 0){
		fprintf(stderr, "[Error] open history error\n");
		return false;
	}
	lseek(history_fd, 0, SEEK_END);

	time_t time_stamp = time(NULL);
	if (puttype == 0){
		sprintf(write_str, "%ld,%s,\"%s\", , \n", time_stamp, sender, text);
	} else if (puttype == 1){
		sprintf(write_str, "%ld,%s,\" \",%s, \n", time_stamp, sender, text);
	} else {
		sprintf(write_str, "%ld,%s,\" \", ,%s\n", time_stamp, sender, text);
	}
	write(history_fd, write_str, strlen(write_str));
	return true;
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
		char filename[BUFSIZE];
		sprintf(filename, "%s/%s", rqst->roomname, rqst->buf + 4);
		strcpy(rqst->filename, filename);
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
		fprintf(stderr, "[Info] filename:%s filesize:%lld\n", rqst->filename, rqst->filesize);

		sprintf(buf, "%lld", rqst->filesize);
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
			fprintf(stderr, "[Info] total write: %lld bytes\n", rqst->offset);
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
	char filename[256], shortname[256];
	int file_fd, writenum;
	int puttype;  // 1:image; 2:file

	if (rqst->state == STATE::LOGIN) {
		fprintf(stderr, "[Info] receive request put from fd %d\n", rqst->connect_fd);
		// get command and file size
		char* start = strtok(rqst->buf + 4, " ");
		if (strstr(start, "image") != NULL){
			puttype = 1;
		} else {
			puttype = 2;
		}
		start = strtok(NULL, " ");
		if (start == NULL){
			send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
			return;
		}
		sprintf(filename, "%s/%s", rqst->roomname, start);
		sprintf(shortname, "%s", start);
		strcpy(rqst->filename, filename);
		start = strtok(NULL, " ");
		if (start == NULL){
			send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
			return;
		}
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
		
		if (WriteHistory(rqst->roomname, rqst->username, shortname, puttype)){
			rqst->offset = 0;
			rqst->state = STATE::PUTTING;
			fprintf(stderr, "[Info] type:%d filename:%s filesize:%lld\n", puttype, rqst->filename, rqst->filesize);
			send(rqst->connect_fd, OK, strlen(OK), 0);
		} else {
			send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
		}
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


void ListAllFriends(request* rqst){
	/*
		After receive the command, find and respond all friend names in one block
	*/
	fprintf(stderr, "[Info] receive request ListAllFriends from fd %d\n", rqst->connect_fd);
	char buf[BUFSIZE];
	memset(buf, '\0', BUFSIZE);
	int offset = 0;
	for (int i = 0; i < rqst->friendnum; i++){
		sprintf(buf+offset, "%s\n", (rqst->friendList)[i]->username);
		offset += strlen((rqst->friendList)[i]->username) + 1;
	}
	if (strlen(buf) > 0){
		send(rqst->connect_fd, buf, strlen(buf), 0);	
	} else {
		send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
	}
	return;
}

void AddFriend(request* rqst){
	fprintf(stderr, "[Info] receive request AddFriend from fd %d\n", rqst->connect_fd);
	char friendname[USERNAME_LIMIT];
	strcpy(friendname, rqst->buf + 10);
	friendname[strlen(friendname) - 1] = '\0';
	int friendidx = -1;
	for (int i = 0; i < MAXFD; i++){
		if (strncmp(friendname, requestList[i].username, USERNAME_LIMIT) == 0){
			if (requestList[i].state != STATE::UNREGISTERED){
				friendidx = i;
			}
			break;
		}
	}
	if (friendidx >= 0){
		for (int i = 0; i < rqst->friendnum; i++){
			if (strcmp((rqst->friendList)[i]->username, requestList[friendidx].username) == 0) {
				// We are already friends
				send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
				fprintf(stderr, "[Info] Fail to add friend: Friendname already exist in friendList\n");
				return;
			}
		}
		(rqst->friendList).push_back(&requestList[friendidx]);
		rqst->friendnum += 1;
		if (strcmp(rqst->username, friendname) != 0){
			(requestList[friendidx].friendList).push_back(rqst);
			requestList[friendidx].friendnum += 1;
		}
		fprintf(stderr, "[Info] Add friend %s successfully\n", requestList[friendidx].username);
		send(rqst->connect_fd, OK, strlen(OK), 0);
	} else {
		fprintf(stderr, "[Info] Fail to add friend: Friendname not exist\n");
		send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
	}
	return;
}

void DeleteFriend(request* rqst){
	fprintf(stderr, "[Info] receive request DeleteFriend from fd %d\n", rqst->connect_fd);
	char friendname[USERNAME_LIMIT];
	strcpy(friendname, rqst->buf + 13);
	friendname[strlen(friendname) - 1] = '\0';
	int friendidx = -1;
	if (!isFriend(rqst->username, friendname)){
		fprintf(stderr, "[Info] Fail to delete friend: Friendname not exist\n");
		send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
		return;
	}
	for (int i = 0; i < MAXFD; i++){
		if (strncmp(friendname, requestList[i].username, USERNAME_LIMIT) == 0){
			if (requestList[i].state != STATE::UNREGISTERED){
				friendidx = i;
			}
			break;
		}
	}
	if (friendidx >= 0){
		for (int i = 0; i < rqst->friendnum; i++){
			if (strcmp((rqst->friendList)[i]->username, requestList[friendidx].username) == 0) {
				std::vector <struct Request*>::iterator toerase = rqst->friendList.begin() + i;
				rqst->friendList.erase(toerase);
				rqst->friendnum -= 1;
				break;
			}
		}
		if (strcmp(rqst->username, friendname) != 0) {
			for (int i = 0; i < requestList[friendidx].friendnum; i++){
				if (strcmp((requestList[friendidx].friendList)[i]->username, rqst->username) == 0) {
					std::vector <struct Request*>::iterator toerase = (requestList[friendidx].friendList).begin() + i;
					(requestList[friendidx].friendList).erase(toerase);
					requestList[friendidx].friendnum -= 1;
					break;
				}
			}
		}
		fprintf(stderr, "[Info] Delete friend %s successfully\n", requestList[friendidx].username);
		send(rqst->connect_fd, OK, strlen(OK), 0);
		return;
	} 
	fprintf(stderr, "[Info] Fail to delete friend: Friendname not exist\n");
	send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
	
	return;
}

bool isFriend(char* user1, char* user2){
	int user1idx = -1;
	for (int i = 0; i < MAXFD; i++){
		if (requestList[i].state != STATE::UNREGISTERED && strcmp(requestList[i].username, user1) == 0){
			user1idx = i;
			break;
		}
	}
	if (user1idx >= 0){
		for (int i = 0; i < requestList[user1idx].friendnum; i++){
			if (strcmp((requestList[user1idx].friendList)[i]->username, user2) == 0){
				return true;
			}
		}
	}
	return false;
}

void CreateChatRoom(request* rqst){
	fprintf(stderr, "[Info] receive request CreateChatRoom from fd %d\n", rqst->connect_fd);
	char roomname[64];
	char* start = strtok(rqst->buf + 7, " ");
	strcpy(roomname, start);
	start = strtok(NULL, " ");
	int type_int = atoi(start);
	TYPE type;
	if (type_int == 1){
		type = TYPE::ONEBYONE;
	} else if (type_int == 2){
		type = TYPE::GROUP;
	} else if (type_int == 3){
		type = TYPE::PUBLIC;
	} else{
		fprintf(stderr, "[Error] invalid type %d\n", type_int);
		send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
		return;
	}
	int roomidx = -1;
	for (int i = 0; i < MAXROOMNUM; i++){
		if (strlen(roomList[i].roomname) == 0){
			roomidx = i;
		}
	}
	if (roomidx < 0){
		fprintf(stderr, "[Error] All rooms are occupied");
		send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
	} else {
		switch (type) {
			case TYPE::ONEBYONE: {
				if (isFriend(rqst->username, roomname)){
					char roomname_full[128];
					sprintf(roomname_full, "%s_%s", rqst->username, roomname);
					if (mkdir(roomname_full, S_IRWXU) < 0){
						if (errno != EEXIST){
							fprintf(stderr, "[Error] mkdir error\n");
							send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
							return;
						}
					}
					strcpy(roomList[roomidx].roomname, roomname_full);
					strcpy(roomList[roomidx].invited_username, roomname);
					roomList[roomidx].usernum = 0;
					roomList[roomidx].admin = rqst;
					roomList[roomidx].type = TYPE::ONEBYONE;
					fprintf(stderr, "[Info] Create room %s_%s successfully\n", rqst->username, roomname);
				} else {
					fprintf(stderr, "[Error] Fail to create room: not friend\n");
					send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
					return;
				}
				break;
			}
			case TYPE::GROUP: {
				if (mkdir(roomname, S_IRWXU) < 0){
					if (errno != EEXIST){
						fprintf(stderr, "[Error] mkdir error\n");
						send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
						return;
					}
				}
				strcpy(roomList[roomidx].roomname, roomname);
				roomList[roomidx].usernum = 0;
				roomList[roomidx].admin = rqst;
				roomList[roomidx].type = TYPE::GROUP;
				fprintf(stderr, "[Info] Create room %s successfully\n", roomname);
				break;
			}
			case TYPE::PUBLIC: {
				if (mkdir(roomname, S_IRWXU) < 0){
					if (errno != EEXIST){
						fprintf(stderr, "[Error] mkdir error\n");
						send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
						return;
					}
				}
				strcpy(roomList[roomidx].roomname, roomname);
				roomList[roomidx].usernum = 0;
				roomList[roomidx].admin = rqst;
				roomList[roomidx].type = TYPE::PUBLIC;
				break;
			}
			default: {
				fprintf(stderr, "[Error] invalid type %d\n", type_int);
				send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
				return;
			}
		}
		fprintf(stderr, "[Info] Create room %s_%s successfully\n", rqst->username, roomname);
		send(rqst->connect_fd, OK, strlen(OK), 0);
	}
	return;
}

void ListChatRoom(request* rqst){
	fprintf(stderr, "[Info] receive request ListChatRoom from fd %d\n", rqst->connect_fd);
	char buf[BUFSIZE];
	memset(buf, '\0', BUFSIZE);
	int offset = 0;
	for (int i = 0; i < MAXROOMNUM; i++){
		if (strlen(roomList[i].roomname) != 0) {
			if (roomList[i].type == TYPE::ONEBYONE){
				if (roomList[i].admin == rqst || strcmp(roomList[i].invited_username, rqst->username) == 0){
					sprintf(buf+offset, "%s(ONEBYONE)\n", roomList[i].roomname);
					offset += strlen(roomList[i].roomname) + 11;
				}
			} else if (roomList[i].type == TYPE::GROUP){
				if (roomList[i].admin == rqst || isFriend((roomList[i].admin)->username, rqst->username)){
					sprintf(buf+offset, "%s(GROUP)\n", roomList[i].roomname);
					offset += strlen(roomList[i].roomname) + 8;
				}
			} else if (roomList[i].type == TYPE::PUBLIC){
				sprintf(buf+offset, "%s(PUBLIC)\n", roomList[i].roomname);
				offset += strlen(roomList[i].roomname) + 9;
			}
		}
	}
	if (strlen(buf) > 0){
		send(rqst->connect_fd, buf, strlen(buf), 0);	
	} else {
		send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
	}
	return;
}

void EnterChatRoom(request* rqst){
	fprintf(stderr, "[Info] receive request EnterChatRoom from fd %d\n", rqst->connect_fd);
	if (strlen(rqst->roomname) != 0){
		fprintf(stderr, "[Error] User already in chatroom %s\n", rqst->roomname);
		send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
		return;
	}
	char roomname[64];
	strcpy(roomname, rqst->buf + 6);
	roomname[strlen(roomname) - 1] = '\0';

	for (int i = 0; i < MAXROOMNUM; i++){
		if (strcmp(roomList[i].roomname, roomname) == 0){
			(roomList[i].users).push_back(rqst);
			roomList[i].usernum += 1;
			strcpy(rqst->roomname, roomname);
			fprintf(stderr, "[Info] Enter room %s successfully\n", roomname);
			send(rqst->connect_fd, OK, strlen(OK), 0);
			return;
		}
	}

	send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
	return;
}

void Text(request* rqst){
	fprintf(stderr, "[Info] receive request Text from fd %d\n", rqst->connect_fd);
	char text[BUFSIZE];
	strcpy(text, rqst->buf + 5);
	text[strlen(text) - 1] = '\0';
	if (WriteHistory(rqst->roomname, rqst->username, text, 0)){
		send(rqst->connect_fd, OK, strlen(OK), 0);
	} else{
		send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
	}
	return;
}

void Leave(request* rqst){
	fprintf(stderr, "[Info] receive request Leave from fd %d\n", rqst->connect_fd);
	int roomidx = -1;
	for (int i = 0; i < MAXROOMNUM; i++){
		if (strcmp(rqst->roomname, roomList[i].roomname) == 0){
			roomidx = i;
			break;
		}
	}
	if (roomidx < 0){
		fprintf(stderr, "[Error] Roomname not exist\n");
		send(rqst->connect_fd, ERROR, strlen(ERROR), 0);
	} else{
		for (int i = 0; i < roomList[roomidx].usernum; i++){
			if (strcmp(rqst->username, (roomList[roomidx].users)[i]->username) == 0){
				std::vector <struct Request*>::iterator toerase = (roomList[roomidx].users).begin() + i;
				(roomList[roomidx].users).erase(toerase);
				roomList[roomidx].usernum -= 1;
				break;
			}
		}
		memset(rqst->roomname, '\0', 64);
		send(rqst->connect_fd, OK, strlen(OK), 0);
	}
	return;
}


