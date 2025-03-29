#include "Request.hpp"
#include "Utils.hpp"
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring> // For strstr

Request::Request() {
    clear();
}

Request::~Request() {}

void Request::clear() {
    _method.clear();
    _path.clear();
    _queryString.clear();
    _httpVersion.clear();
    _headers.clear();
    _body.clear();
    _internalBuffer.clear();
    _currentHeaderLine.clear();

    _contentLength = 0;
    _isChunked = false;
    _currentChunkSize = 0;
    _bodyBytesReceived = 0;

    _state = ParsingMethod;
    _errorCode = 0;
}

Request::ParseState Request::parse(const char* buffer, size_t len) {
    if (len == 0 && _state != Complete && _state != Error) {
        return _state; // No new data, state unchanged unless complete/error
    }

    _internalBuffer.insert(_internalBuffer.end(), buffer, buffer + len);

    bool progressMade;
    do {
        progressMade = processBuffer();
    } while (progressMade && _state != Complete && _state != Error);

    // TODO: Integrate progressive body size limit check here or within handlers

    return _state;
}

// Processes the internal buffer, returns true if progress was made, false otherwise
bool Request::processBuffer() {
    if (_state == Error || _state == Complete) {
        return false;
    }

    // Handle states that don't strictly rely on CRLF line endings first
    if (_state == ParsingBody) {
        if (!_isChunked) {
            size_t available = _internalBuffer.size();
            if (available == 0) return false; // Need more data

            ParseState nextState = handleBodyData(_internalBuffer.data(), available);
            if (nextState != _state) {
                size_t consumed = std::min(available, _contentLength - (_bodyBytesReceived - available)); // How much was ACTUALLY consumed in handleBodyData
                _internalBuffer.erase(_internalBuffer.begin(), _internalBuffer.begin() + consumed);
                _state = nextState;
                return true; // Progress made
            } else {
                // Body not yet complete, consumed all available data
                _internalBuffer.clear();
                return true; // Progress was made (consumed buffer)
            }
        } else { // Chunked body logic is handled below within line processing / state transitions
             _state = ParsingChunkSize; // Move to chunked state if not already there
        }
    }
    if (_state == ParsingChunkData) {
         size_t available = _internalBuffer.size();
         if (available == 0) return false; // Need more chunk data
         ParseState nextState = handleChunkedData(_internalBuffer.data(), available);
         // handleChunkedData erases consumed data from _internalBuffer itself
         if (nextState != _state) {
              _state = nextState;
              return true; // State changed, progress made
         }
         // If state is still ParsingChunkData, it means we consumed the available buffer
         // but need more data for the current chunk. Progress was made.
         return true;
    }

    // For other states, look for CRLF
    const char* crlf_sequence = "\r\n";
    std::vector<char>::iterator crlf_it = std::search(
        _internalBuffer.begin(), _internalBuffer.end(),
        crlf_sequence, crlf_sequence + 2
    );

    if (crlf_it == _internalBuffer.end()) {
        // No CRLF found, need more data for line-based states
        // Check for buffer limit to prevent DoS
        // if (_internalBuffer.size() > MAX_HEADER_LINE_SIZE) { _errorCode = 400; _state = Error; return true; }
        return false;
    }

    // Complete line found
    std::string line(_internalBuffer.begin(), crlf_it);
    size_t consumed_len = std::distance(_internalBuffer.begin(), crlf_it) + 2; // Line + CRLF
    ParseState previousState = _state;

    // --- State Machine (Line-based processing) --- 
    switch (_state) {
        case ParsingMethod: // Includes Path and Version
        case ParsingPath:
        case ParsingHttpVersion:
            _state = parseRequestLine(line);
            break;
        case ParsingHeaders:
            if (line.empty()) { // Blank line -> End of headers
                if (_headers.find("host") == _headers.end()) { // Check Host header presence
                    _errorCode = 400; // Bad Request (Host is required in HTTP/1.1)
                    _state = Error;
                } else if (_isChunked) {
                    _state = ParsingChunkSize;
                } else if (_contentLength > 0) {
                    _state = ParsingBody;
                } else {
                    _state = Complete;
                }
            } else {
                _state = parseHeaderLine(line);
            }
            break;
        case ParsingChunkSize:
            _state = handleChunkedData(line.c_str(), 0); // Special call to parse size line
            break;
        case ParsingChunkTrailer: 
             if (line.empty()) {
                 _state = Complete; // End of trailers
             } else {
                _state = handleChunkedData(line.c_str(), 1); // Special call to parse trailer line
             }
             break;
        case ParsingBody:      // Should be handled above
        case ParsingChunkData: // Should be handled above
        case Complete:
        case Error:
            break; // Should not process lines in these states here
    }

    // Consume the processed line + CRLF from the buffer
    _internalBuffer.erase(_internalBuffer.begin(), _internalBuffer.begin() + consumed_len);

    return (_state != previousState || consumed_len > 0); // Return true if state changed or data was consumed
}

// --- Internal Parsing Helpers ---

Request::ParseState Request::parseRequestLine(const std::string& line) {
    std::istringstream iss(line);
    std::string path_part, version_part;

    if (!(iss >> _method)) { _errorCode = 400; return Error; }
    if (!(iss >> path_part)) { _errorCode = 400; return Error; }
    if (!(iss >> version_part)) { _errorCode = 400; return Error; }

    // Check for extra tokens
    std::string extra;
    if (iss >> extra) { _errorCode = 400; return Error; }

    // Validate HTTP version
    if (version_part != "HTTP/1.1" && version_part != "HTTP/1.0") {
        _errorCode = 505; _httpVersion = version_part; return Error;
    }
    _httpVersion = version_part;

    // Separate query string from path
    size_t queryPos = path_part.find('?');
    if (queryPos != std::string::npos) {
        _path = path_part.substr(0, queryPos);
        _queryString = path_part.substr(queryPos + 1);
    } else {
        _path = path_part;
        _queryString.clear();
    }
    // TODO: Basic path validation/normalization if needed

    return ParsingHeaders;
}

void Request::normalizeHeaderKey(std::string& key) const {
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
}

Request::ParseState Request::parseHeaderLine(const std::string& line) {
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos || colonPos == 0) {
        _errorCode = 400; return Error; // Malformed header
    }

    std::string key = line.substr(0, colonPos);
    std::string value = line.substr(colonPos + 1);

    key = Utils::trim(key);
    value = Utils::trim(value);

    if (key.empty()) { _errorCode = 400; return Error; }

    normalizeHeaderKey(key);

    // Handle specific headers immediately
    if (key == "content-length") {
        if (_isChunked) { _errorCode = 400; return Error; } // Cannot have both
        try {
            char* endptr;
            long len = std::strtol(value.c_str(), &endptr, 10);
            if (*endptr != '\0' || len < 0) { throw std::invalid_argument("Invalid length"); }
             _contentLength = static_cast<size_t>(len);
        } catch (...) { _errorCode = 400; return Error; }
    } else if (key == "transfer-encoding") {
        std::string lowerValue = value; Utils::toLower(lowerValue);
        // We only care about the final encoding (HTTP/1.1 allows multiple, comma-separated)
        if (Utils::endsWith(Utils::trim(lowerValue), "chunked")) {
             if (_contentLength > 0) { _errorCode = 400; return Error; } // Cannot have both
            _isChunked = true;
        } else if (lowerValue != "identity") {
             _errorCode = 501; return Error; // We don't support other encodings
        }
    }

    // Store header (potentially overwriting duplicates, last one wins generally)
    _headers[key] = value;
    return ParsingHeaders;
}

Request::ParseState Request::handleBodyData(const char* buffer, size_t len) {
    size_t bytesToAppend = std::min(len, _contentLength - _bodyBytesReceived);

    if (bytesToAppend > 0) {
        _body.insert(_body.end(), buffer, buffer + bytesToAppend);
        _bodyBytesReceived += bytesToAppend;
        // TODO: Add check for client_max_body_size here
        // if (_bodyBytesReceived > maxBodySize) { _errorCode = 413; return Error; }
    }

    return (_bodyBytesReceived >= _contentLength) ? Complete : ParsingBody;
}

// Handles chunk size/data lines and trailer lines
Request::ParseState Request::handleChunkedData(const char* data, size_t len) {
    // State transition based on line content (called from processBuffer)
    if (len == 0) { // data is a std::string line for chunk size
        const std::string line(data);
        char* endptr;
        long size = std::strtol(line.c_str(), &endptr, 16); // Base 16

        // Ignore chunk extensions for now (text after ';')
        while (*endptr != '\0' && *endptr != ';') endptr++; // Find end or semicolon
        if (size < 0 || (endptr == line.c_str() && size == 0 && line != "0")) { // Invalid size format or negative size
             _errorCode = 400; return Error;
        }
        _currentChunkSize = static_cast<size_t>(size);
        _bodyBytesReceived = 0; // Reset for the new chunk
        return (_currentChunkSize == 0) ? ParsingChunkTrailer : ParsingChunkData;
    }
    // State transition for trailer line (called from processBuffer)
    if (len == 1) { // data is a std::string line for trailer
        return parseHeaderLine(std::string(data));
    }

    // Processing chunk data (called from processBuffer with actual data)
    if (_state == ParsingChunkData) {
        size_t bytesToAppend = std::min(len, _currentChunkSize - _bodyBytesReceived);
        if (bytesToAppend > 0) {
            _body.insert(_body.end(), data, data + bytesToAppend);
            _bodyBytesReceived += bytesToAppend;
            _internalBuffer.erase(_internalBuffer.begin(), _internalBuffer.begin() + bytesToAppend);
             // TODO: Add check for client_max_body_size here
             // if (total body size > maxBodySize) { _errorCode = 413; return Error; }
        }

        // If chunk fully read, next state expects CRLF, then chunk size
        return (_bodyBytesReceived == _currentChunkSize) ? ParsingChunkSize : ParsingChunkData;
    }

    return _state; // Should not happen
}

// --- Getters ---
const std::string& Request::getMethod() const { return _method; }
const std::string& Request::getPath() const { return _path; }
const std::string& Request::getQueryString() const { return _queryString; }
const std::string& Request::getHttpVersion() const { return _httpVersion; }
const std::map<std::string, std::string>& Request::getHeaders() const { return _headers; }
const std::vector<char>& Request::getBody() const { return _body; }
Request::ParseState Request::getState() const { return _state; }
int Request::getErrorCode() const { return _errorCode; }

std::string Request::getHeader(const std::string& key) const {
    std::string lowerKey = key;
    normalizeHeaderKey(lowerKey);
    std::map<std::string, std::string>::const_iterator it = _headers.find(lowerKey);
    if (it != _headers.end()) {
        return it->second;
    }
    return "";
}