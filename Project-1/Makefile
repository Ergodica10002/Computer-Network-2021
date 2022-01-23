CC = g++

all: server client

server: server.cpp server_tools.cpp
	$(CC) server.cpp server_tools.cpp -o server

client: client.cpp client_tools.cpp
	$(CC) client.cpp client_tools.cpp -o client

clean:
	rm -f server client
	rm -rf server_dir client_dir

.PHONY: server client clean