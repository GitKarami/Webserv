#include "../includes/Request.hpp"
#include <iostream>
#include <sstream>

Request::Request() : _method(""), _path(""), _version(""), _headers(), _body("") {};

Request::Request(const std::string& other)
{
    std::istringstream stream(other);
    std::string line;

    std::getline(stream, line);
    if (!line.empty() && line[line.length()-1] == '\r')
        line = line.substr(0, line.length()-1);
    parseRequestLine(line);
    parseHeaders(stream);
}

Request &Request::operator=(const Request &other)
{
    _method = other._method;
    _path = other._path;
    _version = other._version;
    _headers = other._headers;
    _body = other._body;
    return *this;
}

void Request::parseRequestLine(const std::string& line) {
    std::istringstream lineStream(line);
    lineStream >> _method >> _path >> _version;
}

void Request::parseHeaders(std::istringstream& stream) {
    std::string line;
    
    // Parse headers until empty line
    while (std::getline(stream, line) && line != "\r" && line != "") {
        // Remove trailing \r if present
        if (line[line.length()-1] == '\r')
            line = line.substr(0, line.length()-1);
        
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            // Skip colon and space after it
            std::string value = line.substr(colonPos + 2);
            _headers[key] = value;
        }
    }

    // Parse body if present
    std::stringstream bodyStream;
    while (std::getline(stream, line)) {
        bodyStream << line << "\n";
    }
    _body = bodyStream.str();
}

std::string Request::getHeader(const std::string& key) const {
    auto it = _headers.find(key);
    return (it != _headers.end()) ? it->second : "";
}

Request::~Request() {};
