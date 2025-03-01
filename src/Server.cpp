#include "../includes/Server.hpp"

Server::Server() : _serverFd(0), _addrLen(sizeof(_serverAddress))
{
    _serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverFd == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        exit(EXIT_FAILURE);
    }
    _serverAddress.sin_family = AF_INET;
    _serverAddress.sin_addr.s_addr = INADDR_ANY;
    _serverAddress.sin_port = htons(8080);

    if (bind(_serverFd, (struct sockaddr *)&_serverAddress, sizeof(_serverAddress)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(_serverFd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    std::cout << "Server started on port 8080" << std::endl;
}

Server::Server(const Server &other) : _serverFd(other._serverFd), _serverAddress(other._serverAddress), _addrLen(other._addrLen){};

Server &Server::operator=(const Server &other)
{
    _serverFd = other._serverFd;
    _serverAddress = other._serverAddress;
    _addrLen = other._addrLen;
    return *this;
}

Server::~Server()
{
    if (_serverFd != -1)
        close(_serverFd);
}

void Server::run()
{
    int newSocket;
    int valread;

    while (true)
    {
        newSocket = accept(_serverFd, (struct sockaddr *)&_serverAddress, (socklen_t *)&_addrLen);
        if (newSocket < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        char buffer[30000] = {0};
        valread = read(newSocket, buffer, 30000);
        if (valread < 0)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }
        std::cout << buffer << std::endl;
        std::string response = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 12\n\nHello world!";
        send(newSocket, response.c_str(), response.size(), 0);
        close(newSocket);
    }
}