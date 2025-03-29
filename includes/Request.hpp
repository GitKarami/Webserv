#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>

class Request {
public:
    enum ParseState {
        ParsingMethod,
        ParsingPath,
        ParsingHttpVersion,
        ParsingHeaders,
        ParsingBody,
        ParsingChunkSize,
        ParsingChunkData,
        ParsingChunkTrailer,
        Complete,
        Error
    };

    Request();
    ~Request();

    ParseState parse(const char* buffer, size_t len);

    // Getters
    const std::string& getMethod() const;
    const std::string& getPath() const;
    const std::string& getQueryString() const;
    const std::string& getHttpVersion() const;
    const std::map<std::string, std::string>& getHeaders() const;
    const std::vector<char>& getBody() const;
    std::string getHeader(const std::string& key) const;
    ParseState getState() const;
    int getErrorCode() const; // e.g., 400, 413, 505

    void clear(); // Reset for next request on persistent connection

private:
    // Request Line
    std::string _method;
    std::string _path;
    std::string _queryString;
    std::string _httpVersion;

    // Headers
    std::map<std::string, std::string> _headers;

    // Body
    std::vector<char> _body;
    size_t _contentLength;
    bool _isChunked;
    size_t _currentChunkSize;
    size_t _bodyBytesReceived;

    // Parsing state
    ParseState _state;
    std::string _currentHeaderLine;
    std::vector<char> _internalBuffer; // Buffer for partial reads
    int _errorCode;

    // Internal parsing helper methods
    ParseState parseRequestLine(const std::string& line);
    ParseState parseHeaderLine(const std::string& line);
    ParseState handleBodyData(const char* buffer, size_t len);
    ParseState handleChunkedData(const char* buffer, size_t len);
    void normalizeHeaderKey(std::string& key) const;
    bool processBuffer();
};

#endif // REQUEST_HPP 