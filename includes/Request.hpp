#pragma once

#include <string>
#include <map>

class  Request {
private:
    std::string _method;
    std::string _path;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;

public:
    Request();
    Request(const std::string& other);
    Request &operator=(const Request &other);
    ~Request();


    
    std::string getMethod() const { return _method; }
    std::string getPath() const { return _path; }
    std::string getVersion() const { return _version; }
    std::string getHeader(const std::string& key) const;
    std::string getBody() const { return _body; }

private:
    void parseRequestLine(const std::string& line);
    void parseHeaders(std::istringstream& stream);
};
