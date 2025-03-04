#include "Server.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

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
    parseConfig();
}