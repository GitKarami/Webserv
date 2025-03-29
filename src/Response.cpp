#include "Response.hpp"
#include "Utils.hpp"
#include <sstream>
#include <vector>
#include <algorithm> // for std::transform

Response::Response() {
    clear();
}

Response::~Response() {}

void Response::clear() {
    _httpVersion = "HTTP/1.1";
    _statusCode = 200;
    _statusMessage = Utils::getHttpStatusMessage(_statusCode);
    _headers.clear();
    _body.clear();
    _rawResponse.clear();
    _bytesSent = 0;
}

void Response::setStatusCode(int code) {
    _statusCode = code;
    _statusMessage = Utils::getHttpStatusMessage(code);
}

void Response::setHttpVersion(const std::string& version) {
    _httpVersion = version;
}

void Response::setHeader(const std::string& key, const std::string& value) {
    // Normalize header key for consistent checking (e.g., Content-Length)
    std::string lowerKey = key;
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
    _headers[lowerKey] = value; // Overwrite if exists
}

void Response::setBody(const std::vector<char>& body) {
    _body = body;
}

void Response::setBody(const std::string& body) {
    _body.assign(body.begin(), body.end());
}

void Response::addDefaultHeaders() {
    if (_headers.find("date") == _headers.end()) {
        setHeader("Date", Utils::getCurrentHttpDate());
    }
    if (_headers.find("server") == _headers.end()) {
        setHeader("Server", "Webserv/0.1");
    }
    // Content-Length is crucial
    if (_headers.find("content-length") == _headers.end()) {
        // Only set Content-Length if body is present and not chunked (chunked handled differently)
        // Also, responses like 204 No Content or 304 Not Modified MUST NOT have a body or Content-Length
        bool shouldHaveBody = (_statusCode >= 200 && _statusCode != 204 && _statusCode != 304);
        if (shouldHaveBody) {
             setHeader("Content-Length", std::to_string(_body.size()));
        } else {
            // Ensure body is empty for codes that forbid it
            _body.clear();
        }
    }
    // Add Connection header? (e.g., Connection: close or Connection: keep-alive)
    // Defaulting to close might be simpler initially.
    if (_headers.find("connection") == _headers.end()) {
        setHeader("Connection", "close");
    }
}

void Response::buildResponse() {
    _rawResponse.clear();
    _bytesSent = 0;

    addDefaultHeaders();

    // Status Line
    std::string statusLine = _httpVersion + " " + std::to_string(_statusCode) + " " + _statusMessage + "\r\n";
    _rawResponse.insert(_rawResponse.end(), statusLine.begin(), statusLine.end());

    // Headers
    for (const auto& pair : _headers) {
        std::string headerLine = pair.first + ": " + pair.second + "\r\n";
        // Header keys are already normalized to lowercase in setHeader
        _rawResponse.insert(_rawResponse.end(), headerLine.begin(), headerLine.end());
    }

    // End of Headers
    _rawResponse.push_back('\r');
    _rawResponse.push_back('\n');

    // Body (if any)
    _rawResponse.insert(_rawResponse.end(), _body.begin(), _body.end());
}

const char* Response::getRawResponsePtr() const {
    if (_rawResponse.empty() || _bytesSent >= _rawResponse.size()) {
        return nullptr;
    }
    return _rawResponse.data() + _bytesSent;
}

size_t Response::getRawResponseSize() const {
    return _rawResponse.size();
}

size_t Response::getBytesSent() const {
    return _bytesSent;
}

void Response::bytesSent(size_t bytes) {
    _bytesSent += bytes;
    // Clamp to avoid overflow, although logically should not exceed total size
    if (_bytesSent > _rawResponse.size()) {
        _bytesSent = _rawResponse.size();
    }
}

bool Response::isComplete() const {
    return !_rawResponse.empty() && (_bytesSent >= _rawResponse.size());
} 