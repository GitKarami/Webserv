#include "../includes/Server.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

void Server::parseConfig()
{
    std::ifstream file(_configPath);
    if (!file.is_open())
        throw std::runtime_error("Failed to open config file");

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        std::string key, value;
        std::istringstream iss(line);
        iss >> key >> value;
        if (key == "host")
            _host = value;
        else if (key == "listen")
            _port = std::stoi(value);
        else if (key == "root")
            _root = value;
    }
    file.close();
}
Server::Server(const std::string& conf)
{
    _configPath = conf;
    _port = 8080;
    _host = "127.0.0.1";
    _root = "/var/www/html";
    parseConfig();
    setupSocket();
}

void Server::setupSocket()
{
    _socket.bind(_host, _port);
    _socket.listen(10);
    std::cout << "Server listening on " << _host << ":" << _port << std::endl;
}

void Server::handleRequest(int clientSocket)
{
    char buffer[2048];
    int bytesRead = read(clientSocket, buffer, sizeof(buffer));
    if (bytesRead < 0)
        throw std::runtime_error("Failed to read from socket");
    std::string Rawrequest(buffer);
    Request request(Rawrequest);
    Response response;
    response.setStatusCode(200);
    response.setHeader("Content-Type", "text/html");
    response.setBody("<html><body><h1>Hello, World!</h1></body></html>");
    std::string responseString = response.getResponse();
    send(clientSocket, responseString.c_str(), responseString.size(), 0);
}

void Server::run()
{
    std::cout << "Server running" << std::endl;
    while (true)
    {
        int clientSocket = _socket.accept();
        handleRequest(clientSocket);
        std::cout << "Connection accepted" << std::endl;
        close(clientSocket);
    }
}