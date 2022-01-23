# CN2021 Project Phase II

This is a simple tutorial for executing my chatroom program.

Note:
The chatting system supports usage by terminal or browser. They have a similar interface, so only the terminal version is introduced below.

[TOC]

A demo video on Youtube is given: [CN2021 Project Phase2 Demo](https://www.youtube.com/watch?v=tzw7LjD0axY)

## Design

In this system, three types of chatrooms are given. Users can choose the type when creating a room.
1. `ONEBYONE`
The room with `ONEBYONE` type is only visible to user that creator wants to talk to. The room's name should be set to that user, otherwise the creation will fail. The specified user should be one of the creater's friend. Also notice that the room will not be deleted or invisible even when the friendship ends afterwards. That is, we can only create a room for a friend, and we can still retrieve past history with someone who was ever our friend before.
2. `GROUP`
Room with type `GROUP` can only be visible by every creater's friends. The room's name is not limited as in `ONEBYONE` case. Any name not exceed length 64 is OK.
3. `PUBLIC`
Room with type `PUBLIC` is visible to every user in used. The room's name is also not limited.

## Compile

Just enter
```makefile
make clean
make
```
Executable files `server` and `client` will be generated.

## Execute

### Server
```bash
./server [port]
```
The server will use `port` to receive remote login and requests. It also creates a directory `server_dir` for storing and processing data for all chatrooms.

### Client
```bash
./client [server_ip:server_port] [browser_port]
```
The client will open the port `browser_port` to communicate with the browser. The server should be running on `server_ip:server_port`. A directory named `client_dir` is also created to store and processing data for chatrooms.

## Client Usage

### Login
After first executed, the client shows a login screen to ask user to enter username. If the username was used before, user can retrieve the past data, including the chatting history, images, and files.

### Home
After login, there are 6 functions for user to choose from, as the following shows. Enter the number of the command to use.
```
Home
(1) List all friends
(2) Add a friend
(3) Delete a friend
(4) Create a chat room
(5) Enter a chat room
(6) Logout and exit
Enter your command:
```
1. List all friends: Enter 1. User can list all his/her friends.
2. Add a friend: Enter 2. The client will ask user to enter the friend's name. If all goes well, a message like below will be shown.
```
Enter your command:2
Enter your friend's name:
Alice
Add friend Alice successfully
```
3. Delete a friend: Enter 3. The client will ask user to enter the friend's name. The entered name must be user's friend. If all goes well, a message like below will be shown.
```
Enter your command:3
Enter your friend's name:
Alice
Delete friend Alice successfully
```
4. Create a chatroom: Enter 4. The client will ask user to enter a room's name or a friend's name, corresponding to the room's type `GROUP` and `PUBLIC`, or `ONEBYONE`, respectively. Then user should enter the room's type he/she wants to create. Notice that if user choose (1)`ONEBYONE`, the name given before should be one of the user's friends. If all goes well, a message like below will be shown.
```
Enter your command:4
Enter the chatroom's name you want to create, or who you want to chat:
Alice
Enter the chatroom's type you want to create:
(1) ONEBYONE  (2) GROUP  (3) PUBLIC
1
Create room Alice_Alice successfully
```

5. Enter a chat room: Enter 5. The client will show the user all rooms the user has permission to enter and their types, as the following shows.
```
Enter your command:5
(1) Alicepublic(PUBLIC)  (2) Alicegroup(GROUP)  (3) Alice_Alice(ONEBYONE)
Enter which chatroom you want to enter:2
Enter chatroom Alicegroup successfully
```
6. Logout and exit: Enter 6. The server will save all the login and chatting history, and the client will stop running.

### Room
After choosing a room to enter, there are 6 functions for the user to choose from, as the following shows.
```
You're now at room Alicegroup:
 (1) Refresh chatting history
 (2) Say something
 (3) Send an image
 (4) Send a file
 (5) Download a file
 (6) Leave the chatroom
Enter your command:
```
1. Refresh chatting history: Enter 1. All chatting history beforehand will be shown on the screen. The time will also be given as a feature.
```
Enter your command:1

Alice: Hello, everyone (1/16 19:10:31)
Bob sent image img.jpeg (1/16 19:11:30)
Alice: What a beautiful image (1/16 19:11:58)
```
2. Say something: Enter 2. The client will ask user to enter something he/she wants to send. If all goes well, a message like below will be shown.
```
Enter your command:2
Enter what you want to say: Hello, everyone
[Info] Send text successfully
```
3. Send an image: Enter 3. The client will ask user to enter the file's name. Notice that a directory with the same name as the room is created in `client_dir`, and the sending file should be stored in it. If all goes well, a message like below will be shown.
```
Enter your command:3
Enter the image file name: img.jpeg
[Info] filesize: 128171
upload image img.jpeg successfully
```
4. Send a file: Enter 4. The procedure is similar as 3. The only difference of an image and a file is that, an image will be shown directly in the browser, while the file is displayed as a link for user to download.
5. Download a file: Enter 5. The client will list all files the user can download, and the downloaded file will be saved in the chatroom's directory. If all goes well, a message like below will be shown.
```
Enter your command:5
(1)img.jpeg (sent by Bob at 1/16 19:11:30)

Choose a file to download:
1
Download file img.jpeg successfully
```
Notice that this function is not really useful in web mode, since user can directly click the link to download a file.
6. Leave the chatroom: Enter 6. The user is now leaving the room and back to home.
```
Enter your command:6
[Info] Leave chatroom successfully
Home
 (1) List all friends
 (2) Add a friend
 (3) Delete a friend
 (4) Create a chat room
 (5) Enter a chat room
 (6) Logout and exit
Enter your command:
```

## Browser example
Enter the URL `localhost:browser_port` right after executing the client to access this chatting system via browser.
The login screen, home, room, and chatting hitory example is given below as an example. The usage is similar to the terminal version.



### Login (in browser)

<img src="https://i.imgur.com/CoxGlj9.png" alt="z" style="zoom:40%;" />

### Home (in browser)

<img src="https://i.imgur.com/LzEaALL.png" alt="z" style="zoom:40%;" />

### Room (in browser)

<img src="https://i.imgur.com/Nl8AbZw.jpg" alt="zo" style="zoom:40%;" />