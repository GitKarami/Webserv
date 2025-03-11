#include "../includes/Response.hpp"
#include <sstream>

Response::Response() : _statusCode(200)
{
    // Initialize with default headers
    _headers["Server"] = "WebServ/1.0";
    _headers["Connection"] = "close";
}

Response::Response(int statusCode) : _statusCode(statusCode)
{
    // Initialize with default headers
    _headers["Server"] = "WebServ/1.0";
    _headers["Connection"] = "close";
}

Response::~Response()
{
}

void Response::setStatusCode(int code)
{
    _statusCode = code;
}

int Response::getStatusCode() const
{
    return _statusCode;
}

std::string Response::getStatusText() const
{
    return getStatusTextForCode(_statusCode);
}

void Response::setHeader(const std::string& key, const std::string& value)
{
    _headers[key] = value;
}

std::string Response::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end())
        return it->second;
    return "";
}

const std::map<std::string, std::string>& Response::getHeaders() const
{
    return _headers;
}

void Response::setBody(const std::string& body)
{
    _body = body;
    
    // Auto-set Content-Length header
    std::stringstream ss;
    ss << body.length();
    setHeader("Content-Length", ss.str());
}

std::string Response::getBody() const
{
    return _body;
}

std::string Response::toString() const
{
    std::stringstream response;
    
    // Status line
    response << "HTTP/1.1 " << _statusCode << " " << getStatusTextForCode(_statusCode) << "\r\n";
    
    // Headers
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); 
         it != _headers.end(); ++it)
    {
        response << it->first << ": " << it->second << "\r\n";
    }
    
    // Separator between headers and body
    response << "\r\n";
    
    // Body
    if (!_body.empty())
    {
        response << _body;
    }
    
    return response.str();
}

std::string Response::getStatusTextForCode(int code) const
{
    switch (code)
    {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 206: return "Partial Content";
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 416: return "Range Not Satisfiable";
        case 417: return "Expectation Failed";
        case 418: return "I'm a teapot";
        case 426: return "Upgrade Required";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default: return "Unknown Status";
    }
}

std::string Response::getResponse() const
{
    std::stringstream responseStream;
    
    // Status line
    responseStream << "HTTP/1.1 " << _statusCode << " " << getStatusTextForCode(_statusCode) << "\r\n";
    
    // Headers
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); 
         it != _headers.end(); ++it)
    {
        responseStream << it->first << ": " << it->second << "\r\n";
    }
    
    // Empty line between headers and body
    responseStream << "\r\n";
    
    // Body
    if (!_body.empty())
    {
        responseStream << _body;
    }
    
    return responseStream.str();
}