#include "../includes/Response.hpp"
#include <sys/socket.h>

Response::Response() : _statusCode(200), _version("HTTP/1.1") {
    setStatusMessage();
}

Response::Response(int statusCode) : _statusCode(statusCode), _version("HTTP/1.1") {
    setStatusMessage();
}

Response::~Response() {}

void Response::setStatusCode(int code) {
    _statusCode = code;
    setStatusMessage();
}

void Response::setHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
}

void Response::setBody(const std::string& body) {
    _body = body;
    setHeader("Content-Length", std::to_string(body.length()));
}

void Response::setVersion(const std::string& version) {
    _version = version;
}

int Response::getStatusCode() const {
    return _statusCode;
}

std::string Response::getStatusMessage() const {
    return _statusMessage;
}

std::string Response::getHeader(const std::string& key) const {
    auto it = _headers.find(key);
    return (it != _headers.end()) ? it->second : "";
}

std::string Response::getBody() const {
    return _body;
}

void Response::setStatusMessage() {
    _statusMessage = getDefaultStatusMessage(_statusCode);
}

std::string Response::getDefaultStatusMessage(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "Unknown";
    }
}

std::string Response::toString() const {
    std::stringstream response;
    
    // Status line
    response << _version << " " << _statusCode << " " << _statusMessage << "\r\n";
    
    // Headers
    for (const auto& header : _headers) {
        response << header.first << ": " << header.second << "\r\n";
    }
    
    // Empty line between headers and body
    response << "\r\n";
    
    // Body
    response << _body;
    
    return response.str();
}

void Response::send(int clientSocket) const {
    std::string responseStr = toString();
    ::send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
}

