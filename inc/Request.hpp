#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>
#include <vector>

class Request {
public:
    Request();
    ~Request();

    // Parsing function (placeholder - complex logic needed)
    bool parse(const std::string& rawRequest);

    // Accessors
    const std::string& getMethod() const;
    const std::string& getPath() const;
    const std::string& getVersion() const;
    std::string getHeader(const std::string& key) const; // Case-insensitive lookup?
    const std::map<std::string, std::string>& getHeaders() const;
    const std::string& getBody() const;

    // Mutators (used during parsing or potentially by server)
    void setMethod(const std::string& method);
    void setPath(const std::string& path);
    void setVersion(const std::string& version);
    void addHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);


private:
    std::string _method;
    std::string _path;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;
    // Internal parsing state if needed
};

#endif // REQUEST_HPP 