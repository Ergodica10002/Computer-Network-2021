#include "client_webtools.h"

void getIP(char* hostname, char* ip_addr){
    struct addrinfo hints;
    struct addrinfo *res, *tmp;
    char host[256];

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;

    int ret = getaddrinfo(hostname, NULL, &hints, &res);
    if (ret != 0) {
        fprintf(stderr, "[Error] getaddrinfo: %s\n", gai_strerror(ret));
        exit(1);
    }
    tmp = res;
    getnameinfo(tmp->ai_addr, tmp->ai_addrlen, host, sizeof(host), NULL, 0, NI_NUMERICHOST);
    strcpy(ip_addr, host);
    freeaddrinfo(res);

    return;
}

void init_client(char* addr){
	// Parse address
	char* start = strtok(addr, ":");
	getIP(start, svr.ip_addr);
	start = strtok(NULL, ":");
	svr.port = atoi(start);
	fprintf(stderr, "[Info] connect to host: %s port: %d\n", svr.ip_addr, svr.port);
	svr.fd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(svr.ip_addr);
	servaddr.sin_port = htons(svr.port);

	if (connect(svr.fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		fprintf(stderr, "[Error] connect error\n");
		exit(1);
	}

	if (mkdir("client_dir", S_IRWXU) < 0){
		if (errno != EEXIST){
			fprintf(stderr, "[Error] mkdir error\n");
			exit(1);
		}
	}
	chdir("client_dir");

	return;
}

void init_webserver(unsigned short port){
	struct sockaddr_in servaddr;

	gethostname(websvr.hostname, sizeof(websvr.hostname));
	websvr.port = port;
	websvr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	if (bind(websvr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		fprintf(stderr, "[Error] bind error\n");
		exit(1);
	}
	if (listen(websvr.listen_fd, BACKLOG) < 0){
		fprintf(stderr, "[Error] listen error\n");
		exit(1);
	}

	browser.connect_fd = -1;
	browser.buf_len = 0;

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		fprintf(stderr, "[Error] Catch SIGPIPE error\n");
		exit(1);
	}
	return;
}

int read_response(char* read_buf) {
	/* 
		Empty read_buf first and read socket contents into read_buf
	*/
	memset(read_buf, '\0', BUFSIZE);
	int ret = recv(svr.fd, read_buf, BUFSIZE, 0);
	return ret;
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
	// fprintf(stderr, "[Info] Close connection %d\n", rqst->connect_fd);
	close(rqst->connect_fd);
	rqst->connect_fd = -1;
	rqst->buf_len = 0;
	return;
}

int compare(const void *a, const void *b) {
	return strcmp((char*)a, (char*)b);
}

void ls() {
	/*
		Receive all file names in one block
		Sort the names lexicographically and print
	*/
	char buf[BUFSIZE], filelist[1024][64];
	int readnum, filenum;
	send(svr.fd, "ls", 2, 0);
	readnum = read_response(buf);
	if (strncmp(buf, ERROR, strlen(ERROR)) == 0){
		fprintf(stderr, "[Info] Empty directory\n");
	} else {
		char* start = strtok(buf, "\n");
		filenum = 0;
		while (start != NULL){
			strcpy(filelist[filenum], start);
			filenum += 1;
			start = strtok(NULL, "\n");
		}
		qsort(filelist, filenum, sizeof(filelist[0]), compare);
		for (int i = 0; i < filenum; i++){
			printf("%s\n", filelist[i]);
		}
	}
	return;
}

void get(char* filename) {
	/* 
		At the first request command, receive the file size or error message if not exist
		Server delivers file contents blocks by blocks
		Client responds OK for each block received
	*/
	char write_buf[BUFSIZE], read_buf[BUFSIZE];
	int readnum, writenum;
	off_t offset, filesize;

	int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (file_fd < 0){
		if (errno == EEXIST){
			fprintf(stderr, "[Error] file exist error\n");
		} else if (errno == EACCES){
			fprintf(stderr, "[Error] permission error\n");
		}
		return;
	}

	sprintf(write_buf, "get %s\n", filename);
	send(svr.fd, write_buf, strlen(write_buf), 0); // send the get file command
	read_response(read_buf); // get response of file size or error message
	filesize = atoi(read_buf);
	if (filesize <= 0) {
		if (strncmp(read_buf, ERROR, strlen(ERROR)) == 0) {
			printf("The chatting %s doesn't exist\n", filename);
			remove(filename);
			close(file_fd);
			return;
		} else if (strncmp(read_buf, COMMAND_FORMAT_ERR, strlen(COMMAND_FORMAT_ERR)) == 0) {
			printf("%s", COMMAND_FORMAT_ERR);
			remove(filename);
			close(file_fd);
			return;
		} else {
			read_response(read_buf);
			filesize = atoi(read_buf);
			if (filesize <= 0){
				// fprintf(stderr, "[Error] Zero file size %s\n", read_buf);
				send(svr.fd, ERROR, strlen(ERROR), 0);
				read_response(read_buf);
				close(file_fd);
				return;
			}
		}
	}

	offset = 0;
	while (offset < filesize){
		send(svr.fd, OK, strlen(OK), 0); // send OK response
		readnum = read_response(read_buf); // get response of file contents
		writenum = write(file_fd, read_buf, readnum); // write received contents to file
		offset += writenum;
	}

	close(file_fd);
}

off_t fsize(const char* filename) {
	struct stat st;
	if (stat(filename, &st) == 0) {
		return st.st_size;
	}
	// fprintf(stderr, "[Error] cannot get size of file name %s\n", filename);
	return -1;
}

void put(char* filename) {
	/* 
		At the first request command, send the file size and receive OK response
		Client delivers file contents blocks by blocks
		Server responds OK for each block received
	*/
	char write_buf[BUFSIZE], read_buf[BUFSIZE];
	int readnum, writenum, ret;
	off_t offset, filesize;

	char full_pathname[256];
	sprintf(full_pathname, "%s", filename);
	int file_fd = open(filename, O_RDONLY);
	if (file_fd < 0){
		fprintf(stderr, "[Error] open error\n");
		printf("The %s doesn't exist\n", filename);
		return;
	}
	
	filesize = fsize(filename);
	if (filesize < 0) {
		return;
	}

	sprintf(write_buf, "put %s %lld\n", filename, filesize);
	send(svr.fd, write_buf, strlen(write_buf), 0); // send the put file command
	read_response(read_buf); // wait for OK response
	if (strcmp(read_buf, ERROR) == 0){
		fprintf(stderr, "[Error] send file info error\n");
		return;
	}

	offset = 0;
	while (offset < filesize){
		readnum = read(file_fd, read_buf, sizeof(read_buf));
		writenum = send(svr.fd, read_buf, readnum, 0); // send OK response
		do{
			ret = read_response(read_buf);
		} while (ret == 0); // wait for OK response
		offset += writenum;
		if (readnum != writenum) {
			lseek(file_fd, offset, SEEK_SET);
		}
	}

	close(file_fd);
}

void printhome(void){
	printf("Home\n");
	printf(" (1) List all friends\n");
	printf(" (2) Add a friend\n");
	printf(" (3) Delete a friend\n");
	printf(" (4) Create a chat room\n");
	printf(" (5) Enter a chat room\n");
	printf(" (6) Logout and exit\n");
	printf("Enter your command:");
	fflush(stdout);
	return;
}

void printchatroom(char* roomname){
	printf("You're now at room %s:\n", roomname);
	printf(" (1) Refresh chatting history\n");
	printf(" (2) Say something\n");
	printf(" (3) Send an image\n");
	printf(" (4) Send a file\n");
	printf(" (5) Download a file\n");
	printf(" (6) Leave the chatroom\n");
	printf("Enter your command:");
	fflush(stdout);
	return;
}

void ListAllFriends(char* html_filename){
	/*
		Receive all friend names in one block
		Sort the names lexicographically and print
	*/
	char buf[BUFSIZE], friendlist[1024][USERNAME_LIMIT], write_buf[BUFSIZE];
	int readnum, friendnum;
	send(svr.fd, "ListAllFriends", 14, 0);
	readnum = read_response(buf);
	if (strncmp(buf, ERROR, strlen(ERROR)) == 0){
		sprintf(write_buf, "[Error] You have no friends\n");
	} else{
		char* start = strtok(buf, "\n");
		friendnum = 0;
		while (start != NULL){
			strcpy(friendlist[friendnum], start);
			friendnum += 1;
			start = strtok(NULL, "\n");
		}
		qsort(friendlist, friendnum, sizeof(friendlist[0]), compare);
		sprintf(write_buf, "friend number: %d\n", friendnum);
		for (int i = 0; i < friendnum; i++){
			sprintf(buf, "(%d) %s  ", i + 1, friendlist[i]);
			strcat(write_buf, buf);
		}
	}
	printf("%s\n", write_buf);
	Add_info_to_Home(html_Home_page, write_buf, html_filename);
	return;
}

void AddFriend(char* html_filename){
	char buf[BUFSIZE], write_buf[BUFSIZE];
	char friendname[USERNAME_LIMIT], cmdstr[80];
	printf("Enter your friend's name:\n");
	strcpy(html_filename, html_Home_AddFriend_page);
	send_http_file(browser.connect_fd, html_filename, http_content_type_html, 0);

	int fdnum; // for select() to return number of new requests
	fd_set readset; // for select() to know which fd to listen
	struct timeval timeout; // for select() to wait
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

	while (1) {
		FD_ZERO(&readset);
		FD_SET(STDIN_FILENO, &readset);
		FD_SET(browser.connect_fd, &readset);
		fdnum = select(MAXFD, &readset, NULL, NULL, &timeout); // find whether a new request comes
		if (FD_ISSET(STDIN_FILENO, &readset)) {
			fgets(friendname, USERNAME_LIMIT, stdin);
			friendname[strlen(friendname) - 1] = '\0';
			break;
		} else if (FD_ISSET(browser.connect_fd, &readset)) {
			int ret, http_type;
			char filename[64], http_data[BUFSIZE];
			ret = read_request(&browser);
			if (ret <= 0){
				close_connect(&browser);
				return;
			}
			parse_http_request(browser.buf, &http_type, filename, http_data);
			if (http_type == 1 && strstr(http_data, "friendname") != NULL){
				strcpy(friendname, http_data + 11);
				break;
			} else if (strlen(filename) != 0){
				const char*content_type_str;
				if (strstr(filename, "jpeg") != NULL || strstr(filename, "ico") != NULL){
					content_type_str = http_content_type_img;
				} else {
					content_type_str = http_blank_str;
				}
				send_http_file(browser.connect_fd, filename, content_type_str, 0);
			}
		}
	}
	
	sprintf(cmdstr, "AddFriend %s\n", friendname);
	send(svr.fd, cmdstr, strlen(cmdstr), 0);
	read_response(buf); // get response of OK or error message
	if (strncmp(buf, OK, strlen(OK)) == 0) {
		sprintf(write_buf, "Add friend %s successfully\n", friendname);
	} else{
		sprintf(write_buf, "[Error] Fail to add friend %s\n", friendname);
	}

	printf("%s\n", write_buf);
	Add_info_to_Home(html_Home_page, write_buf, html_filename);
	return;
}

void DeleteFriend(char* html_filename){
	char buf[BUFSIZE], write_buf[BUFSIZE];
	char friendname[USERNAME_LIMIT], cmdstr[80];
	printf("Enter your friend's name:\n");

	strcpy(html_filename, html_Home_DeleteFriend_page);
	send_http_file(browser.connect_fd, html_filename, http_content_type_html, 0);

	int fdnum; // for select() to return number of new requests
	fd_set readset; // for select() to know which fd to listen
	struct timeval timeout; // for select() to wait
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

	while (1) {
		FD_ZERO(&readset);
		FD_SET(STDIN_FILENO, &readset);
		FD_SET(browser.connect_fd, &readset);
		fdnum = select(MAXFD, &readset, NULL, NULL, &timeout); // find whether a new request comes
		if (FD_ISSET(STDIN_FILENO, &readset)) {
			fgets(friendname, USERNAME_LIMIT, stdin);
			friendname[strlen(friendname) - 1] = '\0';
			break;
		} else if (FD_ISSET(browser.connect_fd, &readset)) {
			int ret, http_type;
			char filename[64], http_data[BUFSIZE];
			ret = read_request(&browser);
			if (ret <= 0){
				close_connect(&browser);
				return;
			}
			parse_http_request(browser.buf, &http_type, filename, http_data);
			if (http_type == 1 && strstr(http_data, "friendname") != NULL){
				strcpy(friendname, http_data + 11);
				break;
			} else if (strlen(filename) != 0){
				const char*content_type_str;
				if (strstr(filename, "jpeg") != NULL || strstr(filename, "ico") != NULL){
					content_type_str = http_content_type_img;
				} else {
					content_type_str = http_blank_str;
				}
				send_http_file(browser.connect_fd, filename, content_type_str, 0);
			}
		}
	}
	
	sprintf(cmdstr, "DeleteFriend %s\n", friendname);
	send(svr.fd, cmdstr, strlen(cmdstr), 0);
	read_response(buf); // get response of OK or error message
	if (strncmp(buf, OK, strlen(OK)) == 0) {
		sprintf(write_buf, "Delete friend %s successfully\n", friendname);
	} else{
		sprintf(write_buf, "[Error] Fail to delete friend %s\n", friendname);
	}

	printf("%s\n", write_buf);
	Add_info_to_Home(html_Home_page, write_buf, html_filename);
	return;
}

void CreateChatRoom(char* username, char* html_filename){
	char roomname[64];
	char type[3];
	bool room_received = false;

	strcpy(html_filename, html_Home_CreateChatRoom_page);
	send_http_file(browser.connect_fd, html_filename, http_content_type_html, 0);

	int fdnum; // for select() to return number of new requests
	fd_set readset; // for select() to know which fd to listen
	struct timeval timeout; // for select() to wait
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

	printf("Enter the chatroom's name you want to create, or who you want to chat:\n");

	while (1) {
		FD_ZERO(&readset);
		FD_SET(STDIN_FILENO, &readset);
		FD_SET(browser.connect_fd, &readset);
		fdnum = select(MAXFD, &readset, NULL, NULL, &timeout); // find whether a new request comes
		if (FD_ISSET(STDIN_FILENO, &readset)) {
			if (room_received) {
				fgets(type, 3, stdin);
				if (atoi(type) >= 1 && atoi(type) <= 3) {
					break;
				} else {
					printf("[Error] invalid type. Please enter again:\n");
					printf("(1) ONEBYONE  (2) GROUP  (3) PUBLIC\n");
				}
			} else {
				room_received = true;
				fgets(roomname, USERNAME_LIMIT, stdin);
				roomname[strlen(roomname) - 1] = '\0';
				printf("Enter the chatroom's type you want to create:\n");
				printf("(1) ONEBYONE  (2) GROUP  (3) PUBLIC\n");
			}
		} else if (FD_ISSET(browser.connect_fd, &readset)) {
			int ret, http_type;
			char filename[64], http_data[BUFSIZE];
			ret = read_request(&browser);
			if (ret <= 0){
				close_connect(&browser);
				return;
			}
			parse_http_request(browser.buf, &http_type, filename, http_data);
			if (http_type == 1 && strstr(http_data, "roomname") != NULL && strstr(http_data, "roomtype") != NULL){
				strcpy(roomname, strstr(http_data, "roomname") + 9);
				strncpy(type, strstr(http_data, "roomtype") + 9, 3);
				char* AND = strchr(roomname, '&');
				*AND = '\0';
				if (atoi(type) >= 1 && atoi(type) <= 3) {
					break;
				} else {
					fprintf(stderr, "[Error] invalid type %s\n", type);
				}
			} else if (strlen(filename) != 0){
				const char*content_type_str;
				if (strstr(filename, "jpeg") != NULL || strstr(filename, "ico") != NULL){
					content_type_str = http_content_type_img;
				} else {
					content_type_str = http_blank_str;
				}
				send_http_file(browser.connect_fd, filename, content_type_str, 0);
			}
		}
	}

	char buf[BUFSIZE], cmdstr[80], write_buf[BUFSIZE];
	sprintf(cmdstr, "create %s %d\n", roomname, atoi(type));
	send(svr.fd, cmdstr, strlen(cmdstr), 0);
	read_response(buf); // get response of OK or error message
	if (strncmp(buf, OK, strlen(OK)) == 0){
		if (atoi(type) == 1){
			sprintf(write_buf, "Create room %s_%s successfully\n", username, roomname);
		} else {
			sprintf(write_buf, "Create room %s successfully\n", roomname);
		}
	} else{
		if (atoi(type) == 1){
			sprintf(write_buf, "[Error] Fail to create room %s_%s\n", username, roomname);	
		} else {
			sprintf(write_buf, "[Error] Fail to create room %s\n", roomname);
		}
	}
	printf("%s", write_buf);
	Add_info_to_Home(html_Home_page, write_buf, html_filename);
	return;	
}

bool EnterChatRoom(char* roomname, char* html_filename){
	char buf[BUFSIZE], write_buf[BUFSIZE];
	char roomlist[MAXROOMNUM][64];
	int roomnum = 0;
	send(svr.fd, "ListRoom", 8, 0);
	read_response(buf);
	if (strncmp(buf, ERROR, strlen(ERROR)) == 0){
		printf("No chatrooms currently\n");
		FILE* html_homefp = fopen(html_Home_page, "r");
		FILE* html_retfp = fopen("../sources/Home_added.html", "w");
		if (html_homefp == NULL || html_retfp == NULL){
			strcpy(html_filename, html_Home_page);
			return false;
		}
		strcpy(html_filename, "../sources/Home_added.html");
		char html_buf[BUFSIZE];
		while(fgets(html_buf, BUFSIZE, html_homefp) != NULL){
			fputs(html_buf, html_retfp);
			if (strstr(html_buf, "adding information") != NULL){
				fputs("<div>\n<p>\n", html_retfp);
				fputs("No chatrooms currently\n", html_retfp);
				fputs("</p>\n</div>\n", html_retfp);
			}
		}
		fclose(html_homefp);
		fclose(html_retfp);
		return false;
	} else{
		char room_display[64];
		write_buf[0] = '\0';
		char* start = strtok(buf, "\n");
		while (start != NULL){
			strcpy(roomlist[roomnum], start);
			start = strtok(NULL, "\n");
			sprintf(room_display, "(%d) %s  ", roomnum + 1, roomlist[roomnum]);
			strcat(write_buf, room_display);
			roomnum += 1;
		}
	}

	printf("%s\n", write_buf);
	Add_info_to_Home(html_Home_EnterChatRoom_page, write_buf, html_filename);
	send_http_file(browser.connect_fd, html_filename, http_content_type_html, 0);

	char cmdstr[80], roomidx_str[5];
	int roomidx;
	printf("Enter which chatroom you want to enter:\n");

	int fdnum; // for select() to return number of new requests
	fd_set readset; // for select() to know which fd to listen
	struct timeval timeout; // for select() to wait
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

	while (1) {
		FD_ZERO(&readset);
		FD_SET(STDIN_FILENO, &readset);
		FD_SET(browser.connect_fd, &readset);
		fdnum = select(MAXFD, &readset, NULL, NULL, &timeout); // find whether a new request comes
		if (FD_ISSET(STDIN_FILENO, &readset)) {
			fgets(roomidx_str, 5, stdin);
			roomidx = atoi(roomidx_str);
			if (roomidx > roomnum || roomidx <= 0){
				printf("[Error] invalid chatroom index, please enter again:\n");
			} else {
				break;
			}
		} else if (FD_ISSET(browser.connect_fd, &readset)) {
			int ret, http_type;
			char filename[64], http_data[BUFSIZE];
			ret = read_request(&browser);
			if (ret <= 0){
				close_connect(&browser);
				return false;
			}
			parse_http_request(browser.buf, &http_type, filename, http_data);
			if (http_type == 1 && strstr(http_data, "roomidx") != NULL){
				strcpy(roomidx_str, http_data + 8);
				roomidx = atoi(roomidx_str);
				if (roomidx > roomnum || roomidx <= 0){
					printf("[Error] invalid chatroom index, please enter again:\n");
				} else {
					break;
				}
			} else if (strlen(filename) != 0){
				const char*content_type_str;
				if (strstr(filename, "jpeg") != NULL || strstr(filename, "ico") != NULL){
					content_type_str = http_content_type_img;
				} else {
					content_type_str = http_blank_str;
				}
				send_http_file(browser.connect_fd, filename, content_type_str, 0);
			}
		}
	}
	
	strcpy(roomname, roomlist[roomidx - 1]);
	char* left_parenthesis = strchr(roomname, '(');
	if (left_parenthesis == NULL){
		fprintf(stderr, "[Error] fail to find chatroom's type\n");
		strcpy(html_filename, html_Home_page);
		return false;
	}
	*left_parenthesis = '\0';
	sprintf(cmdstr, "enter %s\n", roomname);
	send(svr.fd, cmdstr, strlen(cmdstr), 0);
	read_response(buf);
	if (strncmp(buf, OK, strlen(OK)) == 0){
		printf("Enter chatroom %s successfully\n", roomname);
		if (mkdir(roomname, S_IRWXU) < 0){
			if (errno != EEXIST){
				fprintf(stderr, "[Error] mkdir error\n");
			}
		}
		chdir(roomname);
		write_buf[0] = '\0';
		Add_info_to_Room(html_Room_page, roomname, write_buf, html_filename);
		return true;
	} else{
		sprintf(write_buf, "[Error] fail to enter chatroom %s\n", roomname);
		Add_info_to_Home(html_Home_page, write_buf, html_filename);
		return false;
	}
}

void get_localtime(char* time, char* loc_time){
	time_t calendar_t = (time_t)atoi(time);
	struct tm *nPtr = localtime(&calendar_t);
    int month = nPtr->tm_mon + 1;
    int mday = nPtr->tm_mday;
    int hour = nPtr->tm_hour;
    int min = nPtr->tm_min;
    int sec = nPtr->tm_sec;
    sprintf(loc_time, "%d/%d %d:%d:%d", month, mday, hour, min, sec);
    return;
}

void ShowHistory(char* roomname, char* html_filename){
	get(HISTORY);
	char time_list[256][32], sender_list[256][USERNAME_LIMIT], text_list[256][256], image_list[256][64], file_list[256][64];
	int num = getHistory(time_list, sender_list, text_list, image_list, file_list);
	char write_buf[BUFSIZE], temp[256];
	write_buf[0] = '\0';
	for (int i = 0; i < num; i++){
		if (image_list[i][0] != '\0'){
			printf("%s sent image %s (%s)\n", sender_list[i], image_list[i], time_list[i]);
			sprintf(temp, "%s sent image %s (%s)\n <img src=\"%s\"> <br>", sender_list[i], image_list[i], time_list[i], image_list[i]);
		} else if (file_list[i][0] != '\0'){
			printf("%s sent file %s (%s)\n", sender_list[i], file_list[i], time_list[i]);
			sprintf(temp, "<br> %s sent file <a href=\"%s\">%s</a> (%s)\n<br>", sender_list[i], file_list[i], file_list[i], time_list[i]);
		} else{
			printf("%s: %s (%s)\n", sender_list[i], text_list[i], time_list[i]);
			sprintf(temp, "<br> %s: %s (%s)\n <br>", sender_list[i], text_list[i], time_list[i]);
		}
		strcat(write_buf, temp);
	}
	Add_info_to_Room(html_Room_page, roomname, write_buf, html_filename);
	remove(HISTORY);
	return;
}

int getHistory(char time_list[256][32], char sender_list[256][USERNAME_LIMIT], char text_list[256][256], char image_list[256][64], char file_list[256][64]){
	int count = 0;
	char time[32], loc_time[32], sender[USERNAME_LIMIT], text[BUFSIZE], image[256], file[256];
	char buf[BUFSIZE], backup[BUFSIZE];
	FILE* history_fp = fopen(HISTORY, "r");
	if (history_fp == NULL) {
		printf("[Error] Open history error.\n");
		return 0;
	}
	while(fgets(buf, BUFSIZE, history_fp) != NULL){
		strcpy(backup, buf);
		char* start = strtok(buf, ",");
		if (start != NULL){
			strcpy(time, start);
		}
		start = strtok(NULL, ",");
		if (start != NULL){
			strcpy(sender, start);
		}
		char* left_quote = strchr(backup, '\"');
		char* right_quote = strchr(left_quote + 1, '\"');
		if (left_quote != NULL && right_quote != NULL){
			int len = right_quote - left_quote - 1;
			strncpy(text, left_quote + 1, len);
			text[len] = '\0';
		}
		start = strtok(right_quote + 1, ",");
		if (start != NULL){
			strcpy(image, start);
		}
		start = strtok(NULL, ",");
		if (start != NULL){
			strcpy(file, start);
		}
		get_localtime(time, loc_time);
		strcpy(time_list[count], loc_time);
		strcpy(sender_list[count], sender);
		if (image[0] != ' '){
			strcpy(image_list[count], image);
			file_list[count][0] = '\0';
			text_list[count][0] = '\0';
		} else if (file[0] != ' '){
			if (file[strlen(file) - 1] == '\n') {
				file[strlen(file) - 1] = '\0';
			}
			strcpy(file_list[count], file);
			image_list[count][0] = '\0';
			text_list[count][0] = '\0';
		} else{
			strcpy(text_list[count], text);
			file_list[count][0] = '\0';
			image_list[count][0] = '\0';
		}
		count += 1;
	}
	fclose(history_fp);
	return count;
}

void Refresh(char* html_filename){
	get(HISTORY);
	printf("Refresh successfully\n");
	return;
}

void Text(char* roomname, char* html_filename){
	char buf[BUFSIZE], text[BUFSIZE - 10], cmdstr[BUFSIZE], write_buf[BUFSIZE];

	Add_info_to_Room(html_Room_Text_page, roomname, write_buf, html_filename);
	send_http_file(browser.connect_fd, html_filename, http_content_type_html, 0);

	printf("Enter what you want to say: ");
	fflush(stdout);

	int fdnum; // for select() to return number of new requests
	fd_set readset; // for select() to know which fd to listen
	struct timeval timeout; // for select() to wait
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

	while (1) {
		FD_ZERO(&readset);
		FD_SET(STDIN_FILENO, &readset);
		FD_SET(browser.connect_fd, &readset);
		fdnum = select(MAXFD, &readset, NULL, NULL, &timeout); // find whether a new request comes
		if (FD_ISSET(STDIN_FILENO, &readset)) {
			fgets(text, BUFSIZE - 10, stdin);
			text[strlen(text) - 1] = '\0';
			break;
		} else if (FD_ISSET(browser.connect_fd, &readset)) {
			int ret, http_type;
			char filename[64], http_data[BUFSIZE];
			ret = read_request(&browser);
			if (ret <= 0){
				close_connect(&browser);
				return;
			}
			parse_http_request(browser.buf, &http_type, filename, http_data);
			if (http_type == 1 && strstr(http_data, "Text") != NULL){
				strcpy(text, http_data + 5);
				break;
			} else if (strlen(filename) != 0){
				const char*content_type_str;
				if (strstr(filename, "jpeg") != NULL || strstr(filename, "ico") != NULL){
					content_type_str = http_content_type_img;
				} else {
					content_type_str = http_blank_str;
				}
				send_http_file(browser.connect_fd, filename, content_type_str, 0);
			}
		}
	}

	char* addsymbol = text;
	for (int i = 0; i < strlen(text); i++){
		if (*(addsymbol + i) == '+'){
			*(addsymbol + i) = ' ';
		}
	}
	sprintf(cmdstr, "text %s\n", text);
	send(svr.fd, cmdstr, strlen(cmdstr), 0);
	read_response(buf);
	if (strncmp(buf, OK, strlen(OK)) == 0){
		fprintf(stderr, "[Info] Send text successfully\n");
	} else{
		fprintf(stderr, "[Error] Fail to send text\n");
	}
	ShowHistory(roomname, html_filename);
	return;
}

void Image(char* roomname, char* html_filename){
	/* 
		At the first request command, send the file size and receive OK response
		Client delivers file contents blocks by blocks
		Server responds OK for each block received
	*/
	char write_buf[BUFSIZE], read_buf[BUFSIZE], filename[64];
	int readnum, writenum;
	off_t offset, filesize;

	Add_info_to_Room(html_Room_File_page, roomname, write_buf, html_filename);
	send_http_file(browser.connect_fd, html_filename, http_content_type_html, 0);

	printf("Enter the image file name: ");
	fflush(stdout);

	int fdnum; // for select() to return number of new requests
	fd_set readset; // for select() to know which fd to listen
	struct timeval timeout; // for select() to wait
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

	while (1) {
		FD_ZERO(&readset);
		FD_SET(STDIN_FILENO, &readset);
		FD_SET(browser.connect_fd, &readset);
		fdnum = select(MAXFD, &readset, NULL, NULL, &timeout); // find whether a new request comes
		if (FD_ISSET(STDIN_FILENO, &readset)) {
			fgets(filename, 64, stdin);
			filename[strlen(filename) - 1] = '\0';
			break;
		} else if (FD_ISSET(browser.connect_fd, &readset)) {
			int ret, http_type;
			char http_filename[64], http_data[BUFSIZE];
			ret = read_request(&browser);
			if (ret <= 0){
				close_connect(&browser);
				return;
			}
			parse_http_request(browser.buf, &http_type, http_filename, http_data);
			if (http_type == 1 && strstr(http_data, "File") != NULL){
				strcpy(filename, http_data + 5);
				break;
			}
		}
	}

	int file_fd = open(filename, O_RDONLY);
	filesize = fsize(filename);
	if (filesize < 0 || file_fd < 0) {
		fprintf(stderr, "[Error] open error\n");
		sprintf(write_buf, "The %s doesn't exist\n", filename);
		Add_info_to_Room(html_Room_page, roomname, write_buf, html_filename);
		return;
	}
	fprintf(stderr, "[Info] filesize: %lld\n", filesize);
	sprintf(write_buf, "put image %s %lld\n", filename, filesize);
	send(svr.fd, write_buf, strlen(write_buf), 0); // send the put file command
	read_response(read_buf); // wait for OK response
	if (strcmp(read_buf, ERROR) == 0){
		sprintf(write_buf, "[Error] send file info error\n");
		printf("%s", write_buf);
		Add_info_to_Room(html_Room_page, roomname, write_buf, html_filename);
		return;
	}

	offset = 0;
	while (offset < filesize){
		readnum = read(file_fd, read_buf, sizeof(read_buf));
		writenum = send(svr.fd, read_buf, readnum, 0); // send OK response
		read_response(read_buf); // wait for OK response
		offset += writenum;
		if (readnum != writenum) {
			lseek(file_fd, offset, SEEK_SET);
		}
	}
	close(file_fd);

	printf("upload image %s successfully\n", filename);
	ShowHistory(roomname, html_filename);
	return;
}

void File(char* roomname, char* html_filename){
	/* 
		At the first request command, send the file size and receive OK response
		Client delivers file contents blocks by blocks
		Server responds OK for each block received
	*/
	char write_buf[BUFSIZE], read_buf[BUFSIZE], filename[64];
	int readnum, writenum;
	off_t offset, filesize;

	Add_info_to_Room(html_Room_File_page, roomname, write_buf, html_filename);
	send_http_file(browser.connect_fd, html_filename, http_content_type_html, 0);

	printf("Enter the file name: ");
	fflush(stdout);

	int fdnum; // for select() to return number of new requests
	fd_set readset; // for select() to know which fd to listen
	struct timeval timeout; // for select() to wait
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

	while (1) {
		FD_ZERO(&readset);
		FD_SET(STDIN_FILENO, &readset);
		FD_SET(browser.connect_fd, &readset);
		fdnum = select(MAXFD, &readset, NULL, NULL, &timeout); // find whether a new request comes
		if (FD_ISSET(STDIN_FILENO, &readset)) {
			fgets(filename, 64, stdin);
			filename[strlen(filename) - 1] = '\0';
			break;
		} else if (FD_ISSET(browser.connect_fd, &readset)) {
			int ret, http_type;
			char http_filename[64], http_data[BUFSIZE];
			ret = read_request(&browser);
			if (ret <= 0){
				close_connect(&browser);
				return;
			}
			parse_http_request(browser.buf, &http_type, http_filename, http_data);
			if (http_type == 1 && strstr(http_data, "File") != NULL){
				strcpy(filename, http_data + 5);
				break;
			}
		}
	}

	int file_fd = open(filename, O_RDONLY);
	filesize = fsize(filename);
	if (filesize < 0 || file_fd < 0) {
		fprintf(stderr, "[Error] open error\n");
		sprintf(write_buf, "The %s doesn't exist\n", filename);
		Add_info_to_Room(html_Room_page, roomname, write_buf, html_filename);
		return;
	}
	sprintf(write_buf, "put file %s %lld\n", filename, filesize);
	send(svr.fd, write_buf, strlen(write_buf), 0); // send the put file command
	read_response(read_buf); // wait for OK response
	if (strcmp(read_buf, ERROR) == 0){
		sprintf(write_buf, "[Error] send file info error\n");
		printf("%s", write_buf);
		Add_info_to_Room(html_Room_page, roomname, write_buf, html_filename);
		return;
	}

	offset = 0;
	while (offset < filesize){
		readnum = read(file_fd, read_buf, sizeof(read_buf));
		writenum = send(svr.fd, read_buf, readnum, 0); // send OK response
		read_response(read_buf); // wait for OK response
		offset += writenum;
		if (readnum != writenum) {
			lseek(file_fd, offset, SEEK_SET);
		}
	}
	close(file_fd);

	printf("upload file %s successfully\n", filename);
	ShowHistory(roomname, html_filename);

	return;
}

void Download(char* roomname, char* html_filename) {
	get(HISTORY);
	char time_list[256][32], sender_list[256][USERNAME_LIMIT], text_list[256][256], image_list[256][64], file_list[256][64];
	int num = getHistory(time_list, sender_list, text_list, image_list, file_list);
	char filelist[32][256], write_buf[BUFSIZE], temp[256], fileidx_str[8];
	int filecount = 0, fileidx;
	write_buf[0] = '\0';

	for (int i = 0; i < num; i++){
		if (image_list[i][0] != '\0') {
			sprintf(temp, "(%d)%s (sent by %s at %s)\n", filecount+1, image_list[i], sender_list[i], time_list[i]);
			strcat(write_buf, temp);
			strcpy(filelist[filecount], image_list[i]);
			filecount += 1;
		} else if (file_list[i][0] != '\0') {
			sprintf(temp, "(%d)%s (sent by %s at %s)\n", filecount+1, file_list[i], sender_list[i], time_list[i]);
			strcat(write_buf, temp);
			strcpy(filelist[filecount], file_list[i]);
			filecount += 1;
		}
	}
	if (filecount == 0) {
		sprintf(write_buf, "No files currently\n");
		printf("%s", write_buf);
		Add_info_to_Room(html_Room_page, roomname, write_buf, html_filename);
		remove(HISTORY);
		return;
	}

	Add_info_to_Room(html_Room_Download_page, roomname, write_buf, html_filename);
	send_http_file(browser.connect_fd, html_filename, http_content_type_html, 0);
	printf("%s\nChoose a file to download:\n", write_buf);
	fflush(stdout);

	int fdnum; // for select() to return number of new requests
	fd_set readset; // for select() to know which fd to listen
	struct timeval timeout; // for select() to wait
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

	while (1) {
		FD_ZERO(&readset);
		FD_SET(STDIN_FILENO, &readset);
		FD_SET(browser.connect_fd, &readset);
		fdnum = select(MAXFD, &readset, NULL, NULL, &timeout); // find whether a new request comes
		if (FD_ISSET(STDIN_FILENO, &readset)) {
			fgets(fileidx_str, 5, stdin);
			fileidx = atoi(fileidx_str);
			if (fileidx > filecount || fileidx <= 0){
				printf("[Error] invalid file index, please enter again:\n");
			} else {
				break;
			}
		} else if (FD_ISSET(browser.connect_fd, &readset)) {
			int ret, http_type;
			char filename[64], http_data[BUFSIZE];
			ret = read_request(&browser);
			if (ret <= 0){
				close_connect(&browser);
				remove(HISTORY);
				return;
			}
			parse_http_request(browser.buf, &http_type, filename, http_data);
			if (http_type == 1 && strstr(http_data, "fileidx") != NULL){
				strcpy(fileidx_str, http_data + 8);
				fileidx = atoi(fileidx_str);
				if (fileidx > filecount || fileidx <= 0){
					printf("[Error] invalid file index, please enter again:\n");
				} else {
					break;
				}
			} else if (strlen(filename) != 0){
				const char*content_type_str;
				if (strstr(filename, "jpeg") != NULL || strstr(filename, "ico") != NULL){
					content_type_str = http_content_type_img;
				} else {
					content_type_str = http_blank_str;
				}
				printf("\n[Info] send file %s to browser\n", filename);
				send_http_file(browser.connect_fd, filename, content_type_str, 0);
			}
		}
	}

	get(filelist[fileidx - 1]);
	sprintf(write_buf, "Download file %s successfully\n", filelist[fileidx - 1]);
	printf("%s", write_buf);
	Add_info_to_Room(html_Room_page, roomname, write_buf, html_filename);
	remove(HISTORY);
	return;
}

bool LeaveChatRoom(char* roomname, char* html_filename){
	char buf[BUFSIZE];
	send(svr.fd, "leave", 5, 0);
	read_response(buf);
	if (strncmp(buf, OK, strlen(OK)) == 0){
		fprintf(stderr, "[Info] Leave chatroom successfully\n");
		strcpy(html_filename, html_Home_page);
		chdir("..");
		return true;
	} else{
		fprintf(stderr, "[Error] Fail to leave chatroom\n");
		buf[0] = '\0';
		Add_info_to_Room(html_Room_page, roomname, buf, html_filename);
		return false;
	}
}

void parse_http_request(char* http_request, int* http_type, char* filename, char* http_data){
	int linenum = 0;
	bool is_data = false;
	char strline[BUFSIZE];
	memset(http_data, '\0', BUFSIZE);

	char* curLine = http_request;
	char* blank1;
	char* blank2;
   	while(curLine != NULL) {
   		linenum += 1;
   		char* nextLine = strchr(curLine, '\n');
    	if (nextLine != NULL){
    		*nextLine = '\0';	
    	}
    	if (linenum == 1){
			if (strncmp(curLine, "GET", 3) == 0){
				*http_type = 0;
				blank1 = curLine + 4;
			} else {
				*http_type = 1;
				blank1 = curLine + 5;
			}
			blank2 = strchr(blank1, ' ');
			if (blank2 != NULL){
				*blank2 = '\0';
				if (*blank1 == '/'){
					strcpy(filename, blank1 + 1);
				} else {
					strcpy(filename, blank1);
				}
			}
		}
		if (is_data == true){
			strcat(http_data, curLine);
		}
		if (strlen(curLine) <= 1){
			is_data = true;
		}
    	curLine = nextLine ? (nextLine + 1) : NULL;
   	}

	// printf("Request:\n");
	// printf(" type=%d\n", *http_type);
	// printf(" asked filename=%s\n", filename);
	// printf(" data=%s\n", http_data);

	return;
}

void send_http_file(int browser_fd, char* filename, const char* content_type_str, int close_connection) {
	char read_buf[BUFSIZE], write_buf[BUFSIZE * 2], source_filename[64];
	int file_fd = open(filename, O_RDONLY);
	sprintf(source_filename, "../sources/%s", filename);
	int readnum, writenum;
	off_t offset = 0, filesize = fsize(filename);
	if (file_fd < 0) {
		file_fd = open(source_filename, O_RDONLY);
		filesize = fsize(source_filename);
		if (file_fd < 0) {
			get(filename);
			file_fd = open(filename, O_RDONLY);
			filesize = fsize(filename);
			if (file_fd < 0){
				fprintf(stderr, "[Error] Open file error\n");
				sprintf(write_buf, "%s\r\n%s\r\n%s %d\r\n\r\n", \
				http_header_NOTFOUND, http_connection_close, http_content_length, 0);
				send(browser.connect_fd, write_buf, strlen(write_buf), 0);
				return;
			}
		}
	}
	if (close_connection == 1) {
		if (content_type_str[0] == ' '){
			sprintf(write_buf, "%s\r\n%s\r\n%s %lld\r\n\r\n", \
			http_header_OK, http_connection_close, http_content_length, filesize);
		} else {
			sprintf(write_buf, "%s\r\n%s\r\n%s %lld\r\n%s\r\n\r\n", \
			http_header_OK, http_connection_close, http_content_length, filesize, content_type_str);
		}
	} else {
		if (content_type_str[0] == ' ') {
			sprintf(write_buf, "%s\r\n%s %lld\r\n\r\n", \
			http_header_OK, http_content_length, filesize);
		} else {
			sprintf(write_buf, "%s\r\n%s %lld\r\n%s\r\n\r\n", \
			http_header_OK, http_content_length, filesize, content_type_str);
		}
	}
	int header_len = strlen(write_buf);

	// send(browser.connect_fd, write_buf, strlen(write_buf), 0);
	// printf("\nsend header:\n%s\n", write_buf);
	while(offset < filesize) {
		readnum = read(file_fd, read_buf, BUFSIZE);
		if (offset == 0) {
			memcpy(write_buf + header_len, read_buf, readnum);
			send(browser.connect_fd, write_buf, header_len + readnum, 0);
			offset += readnum;
		} else {
			writenum = send(browser_fd, read_buf, readnum, 0); // send OK response
			offset += writenum;
			if (readnum != writenum) {
				lseek(file_fd, offset, SEEK_SET);
			}
		}
	}
	close(file_fd);	
	return;
}

bool Add_info_to_Home(const char* Home_filename, char* write_buf, char* html_filename) {
	bool flag = false;
	FILE* html_homefp = fopen(Home_filename, "r");
	if (html_homefp == NULL) {
		html_homefp = fopen("../sources/Home.html", "r");
		if (html_homefp == NULL) {
			html_homefp = fopen("../../sources/Home.html", "r");
			flag = true;
		}
	}
	FILE* html_retfp = fopen("../sources/Home_added.html", "w");
	strcpy(html_filename, "../sources/Home_added.html");
	if (html_retfp == NULL){
		html_retfp = fopen("../../sources/Home_added.html", "w");
		strcpy(html_filename, "../../sources/Home_added.html");
	}
	if (html_retfp == NULL){
		if (flag){
			strcpy(html_filename, "../../sources/Home.html");
		} else {
			strcpy(html_filename, html_Home_page);
		}
		return false;
	}
	char html_buf[BUFSIZE];
	while(fgets(html_buf, BUFSIZE, html_homefp) != NULL){
		fputs(html_buf, html_retfp);
		if (strstr(html_buf, "adding information") != NULL){
			fputs("<div>\n<p>\n", html_retfp);
			fputs(write_buf, html_retfp);
			fputs("</p>\n</div>\n", html_retfp);
		}
	}
	fclose(html_homefp);
	fclose(html_retfp);
	return true;
}

bool Add_info_to_Room(const char* Room_filename, char* roomname, char* write_buf, char* html_filename) {
	bool flag = false;
	FILE* html_roomfp = fopen(Room_filename, "r");
	if (html_roomfp == NULL) {
		html_roomfp = fopen(html_Room_page, "r");
		if (html_roomfp == NULL) {
			html_roomfp = fopen("../sources/Room.html", "r");
			flag = true;
		}
	}
	FILE* html_retfp = fopen("../../sources/Room_added.html", "w");
	strcpy(html_filename, "../../sources/Room_added.html");
	if (html_retfp == NULL){
		html_retfp = fopen("../sources/Room_added.html", "w");
		strcpy(html_filename, "../sources/Room_added.html");
	}
	if (html_retfp == NULL){
		if (flag) {
			strcpy(html_filename, "../sources/Room.html");
		} else {
			strcpy(html_filename, html_Room_page);	
		}
		return false;
	}
	char html_buf[BUFSIZE];
	while(fgets(html_buf, BUFSIZE, html_roomfp) != NULL){
		fputs(html_buf, html_retfp);
		if (strstr(html_buf, "adding room information") != NULL){
			fputs("<h2> Room ", html_retfp);
			fputs(roomname, html_retfp);
			fputs(" </h2>\n", html_retfp);
		}
		if (strstr(html_buf, "adding information") != NULL){
			fputs("<div>\n<p>\n", html_retfp);
			fputs(write_buf, html_retfp);
			fputs("</p>\n</div>\n", html_retfp);
		}
	}
	fclose(html_roomfp);
	fclose(html_retfp);
	return true;
}