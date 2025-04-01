#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <map>
#include <vector>

class Response {
public:
    Response();
    ~Response();

    // Setters
    void setVersion(const std::string& version);
    void setStatusCode(int code, const std::string& message = "");
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);

    // Getters (optional)
    int getStatusCode() const;
    const std::string& getBody() const;

    // Generate the full HTTP response string
    std::string toString() const;

private:
    std::string _version;
    int _statusCode;
    std::string _statusMessage;
    std::map<std::string, std::string> _headers;
    std::string _body;

    // Helper to get default status message
    std::string getDefaultStatusMessage(int code);
};

#endif // RESPONSE_HPP 