#pragma once

#include <string>
#include <netinet/in.h>

class Socket
{
    private:
        int                 _socketFd;
        struct sockaddr_in  _address;
        int                 _addrLen;
        bool                _isBlocking;
    public:
        Socket();
        ~Socket();

        void        bind(const std::string& host, int port);
        void        listen(int backlog);
        int         accept();
        void        setNonBLocking();
        void        setReuseAddr();

        int         getFd() const;
        std::string getClientIP() const;
        int         getClientPort() const;
};