#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <vector>
#include <map>

class Response {
public:
    Response();
    ~Response();

    void setStatusCode(int code);
    void setHttpVersion(const std::string& version);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::vector<char>& body);
    void setBody(const std::string& body);

    // Builds the raw HTTP response string (status line, headers, body)
    void buildResponse();

    // Getters for sending
    const char* getRawResponsePtr() const; // Pointer to the data to send
    size_t getRawResponseSize() const; // Total size of the response
    size_t getBytesSent() const;
    bool isComplete() const; // Check if entire response has been marked as sent

    // Update send progress
    void bytesSent(size_t bytes);

    void clear(); // Reset for reuse

private:
    std::string _httpVersion;
    int _statusCode;
    std::string _statusMessage;
    std::map<std::string, std::string> _headers;
    std::vector<char> _body;

    std::vector<char> _rawResponse; // Buffer holding the complete response
    size_t _bytesSent;

    void addDefaultHeaders();
};

#endif // RESPONSE_HPP 