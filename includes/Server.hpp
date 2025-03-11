#pragma once

#include <string>
#include <netinet/in.h>
#include <iostream>
#include "../includes/Socket.hpp"
#include "../includes/Request.hpp"
#include "../includes/Response.hpp"

class Server 
{
    public:
        Server(const std::string& conf);
        ~Server() = default;
    
        void run();

    private:
        std::string _configPath;
        std::string _host;
        std::string _root;
        int         _port;

        Socket      _socket;

		void parseConfig();
		void handleRequest(int clientSocket);
        void setupSocket();
};