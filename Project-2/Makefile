CC = g++

all: server client

server: server.cpp server_tools.cpp
	$(CC) -std=c++11 server.cpp server_tools.cpp -o server

client: client_web.cpp client_webtools.cpp
	$(CC) -std=c++11 client_web.cpp client_webtools.cpp -o client

clean:
	rm -f server client
	rm -rf server_dir client_dir

.PHONY: server client clean