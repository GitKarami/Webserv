#include "Request.hpp"
#include <iostream> // Example include
#include <sstream>
#include <algorithm> // for std::transform (lowercase header keys)

Request::Request() {
    // Constructor implementation
}

Request::~Request() {
    // Destructor implementation
}

// Basic placeholder parsing. Real implementation needs robust error handling,
// state management for partial requests, header parsing, body handling (Content-Length).
bool Request::parse(const std::string& rawRequest) {
    std::cout << "--- Parsing Request --- (Placeholder)\n" << rawRequest.substr(0, 100) << "...\n--- End Request Preview ---" << std::endl;

    size_t headers_end = rawRequest.find("\r\n\r\n");
    if (headers_end == std::string::npos) {
        std::cerr << "Request::parse: Headers end (\\r\\n\\r\\n) not found." << std::endl;
        return false; // Not a complete request yet
    }

    std::string request_line_end = rawRequest.substr(0, headers_end);
    std::istringstream requestStream(request_line_end);
    std::string line;

    // 1. Parse Request Line
    if (std::getline(requestStream, line) && !line.empty() && line.back() == '\r') {
         line.pop_back(); // Remove trailing '\r'
        std::istringstream lineStream(line);
        if (!(lineStream >> _method >> _path >> _version)) {
             std::cerr << "Request::parse: Failed to parse request line: " << line << std::endl;
             // Throw exception or return false?
             return false; // Indicate parse failure
        }
        std::cout << "Parsed Request Line: Method=" << _method << ", Path=" << _path << ", Version=" << _version << std::endl;
    } else {
         std::cerr << "Request::parse: Invalid or empty request line." << std::endl;
        return false;
    }

    // 2. Parse Headers (Simplified)
    _headers.clear();
    while (std::getline(requestStream, line) && !line.empty() && line.back() == '\r') {
         line.pop_back(); // Remove trailing '\r'
         if (line.empty()) break; // End of headers (should be handled by find("\r\n\r\n") but good safety)

        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            // Trim whitespace from value
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            // Store header (consider lowercase key for case-insensitivity)
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
            _headers[lowerKey] = value;
             // std::cout << "Parsed Header: " << lowerKey << ": " << value << std::endl;
        } else {
             std::cerr << "Request::parse: Malformed header line: " << line << std::endl;
             // Ignore malformed header? Or fail request? For now, ignore.
        }
    }

    // 3. TODO: Handle Body based on Content-Length or Transfer-Encoding (Chunked)
    // For basic GET, body is usually empty.
    size_t body_start = headers_end + 4; // Start after \r\n\r\n
    if (body_start < rawRequest.length()) {
        // Check Content-Length header
         std::map<std::string, std::string>::const_iterator cl_it = _headers.find("content-length");
        if (cl_it != _headers.end()) {
             try {
                size_t contentLength = std::stoul(cl_it->second);
                 if (rawRequest.length() >= body_start + contentLength) {
                    _body = rawRequest.substr(body_start, contentLength);
                     std::cout << "Parsed Body (" << contentLength << " bytes)." << std::endl;
                 } else {
                      std::cerr << "Request::parse: Incomplete body. Expected " << contentLength << ", got " << (rawRequest.length() - body_start) << std::endl;
                     return false; // Body not fully received yet
                 }
             } catch (...) {
                 std::cerr << "Request::parse: Invalid Content-Length: " << cl_it->second << std::endl;
                 return false; // Bad Request
             }
        }
        // TODO: Handle chunked transfer encoding
    }


    return true; // Parsing successful (for headers at least)
}


// Accessors
const std::string& Request::getMethod() const { return _method; }
const std::string& Request::getPath() const { return _path; }
const std::string& Request::getVersion() const { return _version; }
const std::map<std::string, std::string>& Request::getHeaders() const { return _headers; }
const std::string& Request::getBody() const { return _body; }

std::string Request::getHeader(const std::string& key) const {
    std::string lowerKey = key;
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
    std::map<std::string, std::string>::const_iterator it = _headers.find(lowerKey);
    if (it != _headers.end()) {
        return it->second;
    }
    return ""; // Return empty string if header not found
}

// Mutators
void Request::setMethod(const std::string& method) { _method = method; }
void Request::setPath(const std::string& path) { _path = path; }
void Request::setVersion(const std::string& version) { _version = version; }
void Request::addHeader(const std::string& key, const std::string& value) {
     std::string lowerKey = key;
     std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
    _headers[lowerKey] = value;
}
void Request::setBody(const std::string& body) { _body = body; }

// Implement Request methods here
// e.g., void Request::parse(const std::string& raw_request) { ... } 