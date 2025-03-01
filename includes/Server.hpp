#pragma once

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

class Server {
private:
    int                 _serverFd;
    struct sockaddr_in  _serverAddress;
    int                 _addrLen;
public:
    Server();
    Server(const Server &other);
    Server &operator=(const Server &other);
    ~Server();

    void run();
};