#pragma once

#include <iostream>
#include <map>
#include <sstream>

class Response {
    private:
        int _statusCode;
        std::string _statusMessage;
        std::map<std::string, std::string> _headers;
        std::string _body;
        std::string _version;
    
    public:
        Response();
        Response(int statusCode);
        ~Response();
    
        // Setters
        void setStatusCode(int code);
        void setHeader(const std::string& key, const std::string& value);
        void setBody(const std::string& body);
        void setVersion(const std::string& version);
    
        // Getters
        int getStatusCode() const;
        std::string getStatusMessage() const;
        std::string getHeader(const std::string& key) const;
        std::string getBody() const;
    
        // Utility methods
        std::string toString() const;
        void send(int clientSocket) const;
    
    private:
        void setStatusMessage();
        static std::string getDefaultStatusMessage(int code);
    };