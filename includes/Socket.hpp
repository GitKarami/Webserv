#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <string>

class Socket {
public:
    // Default constructor: Creates an invalid socket
    Socket();

    // Constructor that takes an existing file descriptor (takes ownership)
    explicit Socket(int fd);

    // Destructor: Closes the socket if valid
    ~Socket();

    // Disable copy constructor and assignment operator
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Enable move constructor and assignment operator (C++11)
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // Socket operations
    bool create(int domain = AF_INET, int type = SOCK_STREAM, int protocol = 0);
    bool bind(const struct sockaddr* addr, socklen_t addrlen);
    bool bind(const std::string& host, int port);
    bool listen(int backlog);
    int accept(struct sockaddr* addr, socklen_t* addrlen);
    bool connect(const struct sockaddr* addr, socklen_t addrlen);
    ssize_t send(const void* buf, size_t len, int flags = 0);
    ssize_t recv(void* buf, size_t len, int flags = 0);
    bool close();

    // Socket options
    bool setReuseAddr(bool enable = true);
    bool setNonBlocking(bool enable = true);

    // Getters
    int getFd() const;
    bool isValid() const;

    // Release ownership of the file descriptor
    int release();

private:
    int _fd;
};

#endif // SOCKET_HPP 