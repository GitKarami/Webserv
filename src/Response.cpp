#include "Response.hpp"
#include <iostream> // Example include
#include <sstream>
#include <ctime> // For Date header
#include <algorithm> // <-- Add this include for std::transform

Response::Response() : _version("HTTP/1.1"), _statusCode(200), _statusMessage("OK") {}

Response::~Response() {
    // Destructor implementation
}

void Response::setVersion(const std::string& version) {
    _version = version;
}

void Response::setStatusCode(int code, const std::string& message) {
    _statusCode = code;
    if (message.empty()) {
        _statusMessage = getDefaultStatusMessage(code);
    } else {
        _statusMessage = message;
    }
}

void Response::setHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
}

void Response::setBody(const std::string& body) {
    _body = body;
    // Automatically set Content-Length if not already set? Or require manual setting?
    // Let's require manual setting for now, as done in Server::generateResponse
    // setHeader("Content-Length", std::to_string(body.length()));
}

int Response::getStatusCode() const {
    return _statusCode;
}

const std::string& Response::getBody() const {
    return _body;
}


// Generate the full HTTP response string
std::string Response::toString() const {
    std::ostringstream oss;

    // Status Line
    oss << _version << " " << _statusCode << " " << _statusMessage << "\r\n";

    // Headers
    // Add mandatory headers if not present? (Date, Server)
    bool dateSet = false;
    bool serverSet = false;
    bool connectionSet = false;
    bool contentLengthSet = false;

    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        oss << it->first << ": " << it->second << "\r\n";
        std::string lowerKey = it->first;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
         if (lowerKey == "date") dateSet = true;
         if (lowerKey == "server") serverSet = true;
         if (lowerKey == "connection") connectionSet = true;
         if (lowerKey == "content-length") contentLengthSet = true;
    }

     // Add Content-Length if body is present and header wasn't set manually
     if (!contentLengthSet && !_body.empty()) {
        oss << "Content-Length: " << _body.length() << "\r\n";
     } else if (!contentLengthSet && _body.empty() && _statusCode != 204 && _statusCode != 304) {
         // Add Content-Length: 0 for responses that normally have a body but it's empty
         // Except for 204 No Content and 304 Not Modified
         oss << "Content-Length: 0\r\n";
     }


    // Add Server header if not set
    if (!serverSet) {
        oss << "Server: webserv/0.1 (Custom)" << "\r\n"; // Example server name
    }

    // Add Date header if not set
    if (!dateSet) {
        char buf[100];
        time_t now = time(0);
        struct tm tm = *gmtime(&now);
        strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S GMT", &tm);
        oss << "Date: " << buf << "\r\n";
    }

     // Add Connection header if not set (default to close for HTTP/1.1 simplicity)
    if (!connectionSet) {
         oss << "Connection: close" << "\r\n";
    }


    // End of headers
    oss << "\r\n";

    // Body (if present)
    oss << _body;

    return oss.str();
}

// Helper to get default status messages
std::string Response::getDefaultStatusMessage(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found"; // Or Temporary Redirect
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 503: return "Service Unavailable";
        default: return "Unknown Status";
    }
}

// Implement Response methods here
// e.g., std::string Response::toString() const { ... } 