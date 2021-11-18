# Computer-Network-2021

A simple C-based socket programming project for files upload and download.

## Compile

Use Makefile to compile all codes.
```bash
make clean
make
```

It generates two binary executables: `server` and `client`.

## Usage

### Server

On server side, execute the `server` with a specified port number `[port]`. 1024 < `[port]` < 65536.

```bash
./server [port]
```

A directory `server_dir` will be created in the current directory. Files uploaded from clients are saved in `server_dir`, and clients can download files from it also.

The server is then listening for asked connection and command requests from clients.

### Client

On client side, execute the `client` with a specified IP address and port number `[ip:port]`. The IP address and port number should correspond to that of server.

```bash
./client [ip:port]
```

A directory `client_dir` will be created in the current directory. Files downloaded from the servers are saved in `client_dir`, and clients can upload files to server from it.

Each client first needs to type a username that is not registered in server currently, and after that it can send command requests to the server.

## Command

The server support the following commands for clients to request:

### ls
`ls` command shows all the files in `server_dir`:
```bash
ls
```

### get
Clients use `get` to download files from server.
```bash
get filename
```
The downloaded files are saved in `client_dir`.

### put
Clients use `put` to upload files to server.
```bash
put filename
```
The uploaded files will be saved in `server_dir` on server side.
