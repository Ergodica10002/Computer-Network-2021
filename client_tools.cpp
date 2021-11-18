#include "client_tools.h"

void init_client(char* addr){
	// Parse address
	char* start = strtok(addr, ":");
	strcpy(svr.ip_addr, start);
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

int read_response(char* read_buf) {
	memset(read_buf, '\0', BUFSIZE);
	int ret = recv(svr.fd, read_buf, BUFSIZE, 0);
	return ret;
}

void ls() {
	char buf[BUFSIZE];
	int readnum, file_num;
	send(svr.fd, "ls", 2, 0);
	readnum = read_response(buf);
	if (strncmp(buf, ERROR, strlen(ERROR)) == 0){
		fprintf(stderr, "[Info] Empty directory\n");
	} else {
		printf("%s", buf);
	}
	return;
}

void get(char* filename) {
	char write_buf[BUFSIZE], read_buf[BUFSIZE];
	int readnum, writenum;
	off_t offset, filesize;

	int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (file_fd < 0){
		fprintf(stderr, "[Error] open error\n");
		return;
	}

	sprintf(write_buf, "get %s\n", filename);
	send(svr.fd, write_buf, strlen(write_buf), 0); // send the get file command
	read_response(read_buf); // get response of file size or error message

	filesize = atoi(read_buf);
	if (filesize <= 0) {
		if (strncmp(read_buf, ERROR, strlen(ERROR)) == 0) {
			printf("The %s doesn't exist\n", filename);
			remove(filename);
		} else if (strncmp(read_buf, COMMAND_FORMAT_ERR, strlen(COMMAND_FORMAT_ERR)) == 0) {
			printf("%s", COMMAND_FORMAT_ERR);
			remove(filename);
		} else {
			fprintf(stderr, "[Error] Zero file size\n");
			send(svr.fd, ERROR, strlen(ERROR), 0);
			read_response(read_buf);
		}
		close(file_fd);
		return;
	}
	fprintf(stderr, "[Info] filesize: %ld\n", filesize);

	offset = 0;
	clock_t time_start, time_end;
	time_start = clock();
	while (offset < filesize){
		send(svr.fd, OK, strlen(OK), 0); // send OK response
		readnum = read_response(read_buf); // get response of file contents
		writenum = write(file_fd, read_buf, readnum); // write received contents to file
		offset += writenum;
	}
	time_end = clock();
	fprintf(stderr, "[Info] total write %ld bytes\n", offset);
	fprintf(stderr, "[Info] spend time: %fs\n", (double)(time_end - time_start) / CLOCKS_PER_SEC);
	
	printf("get %s successfully\n", filename);
	close(file_fd);
}

off_t fsize(const char* filename) {
	struct stat st;
	if (stat(filename, &st) == 0) {
		return st.st_size;
	}
	fprintf(stderr, "[Error] file name %s\n", filename);
	return -1;
}

void put(char* filename) {
	char write_buf[BUFSIZE], read_buf[BUFSIZE];
	int readnum, writenum;
	off_t offset, filesize;

	int file_fd = open(filename, O_RDONLY);
	if (file_fd < 0){
		fprintf(stderr, "[Error] open error\n");
		return;
	}
	
	filesize = fsize(filename);
	if (filesize < 0) {
		fprintf(stderr, "[Error] file size error\n");
		return;
	}

	fprintf(stderr, "[Info] filesize: %ld\n", filesize);
	sprintf(write_buf, "put %s %ld\n", filename, filesize);
	send(svr.fd, write_buf, strlen(write_buf), 0); // send the put file command
	read_response(read_buf); // wait for OK response
	if (strcmp(read_buf, ERROR) == 0) {
		fprintf(stderr, "[Error] send file info error\n");
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

	printf("put %s successfully\n", filename);

	close(file_fd);
}