#include "Socket.hpp"
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <iostream> // For error logging
#include <netdb.h>      // For getaddrinfo
#include <arpa/inet.h>  // For inet_ntop

// --- Constructors / Destructor / Move Semantics ---

Socket::Socket() : _fd(-1) {}

Socket::Socket(int fd) : _fd(fd) {
    if (_fd < 0) {
        // Or throw? For now, just mark as invalid.
        _fd = -1;
    }
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept : _fd(other._fd) {
    other._fd = -1; // Prevent double close
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close(); // Close existing socket if any
        _fd = other._fd;
        other._fd = -1;
    }
    return *this;
}

// --- Socket Operations ---

bool Socket::create(int domain, int type, int protocol) {
    if (isValid()) {
        std::cerr << "Socket::create error: Socket already created (fd=" << _fd << ")" << std::endl;
        return false;
    }
    _fd = ::socket(domain, type, protocol);
    if (_fd < 0) {
        std::cerr << "Socket::create error: " << strerror(errno) << std::endl;
        _fd = -1;
        return false;
    }
    return true;
}

bool Socket::bind(const struct sockaddr* addr, socklen_t addrlen) {
    if (!isValid()) {
        std::cerr << "Socket::bind error: Invalid socket" << std::endl;
        return false;
    }
    if (::bind(_fd, addr, addrlen) < 0) {
        std::cerr << "Socket::bind error: " << strerror(errno) << std::endl;
        // Don't close here, let the caller decide based on the error
        return false;
    }
    return true;
}

bool Socket::bind(const std::string& host, int port) {
    if (!isValid()) {
        std::cerr << "Socket::bind error: Invalid socket" << std::endl;
        return false;
    }

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6, or AF_UNSPEC for either
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Fill in my IP for me

    std::string portStr = std::to_string(port);
    int status = getaddrinfo(host.empty() ? NULL : host.c_str(), portStr.c_str(), &hints, &res);
    if (status != 0) {
        std::cerr << "Socket::bind getaddrinfo error: " << gai_strerror(status) << std::endl;
        return false;
    }

    bool bound = false;
    for (p = res; p != NULL; p = p->ai_next) {
        if (::bind(_fd, p->ai_addr, p->ai_addrlen) == 0) {
            bound = true; // Successfully bound
            break;
        }
        // If bind fails, print error but keep trying other addresses from getaddrinfo
        std::cerr << "Socket::bind attempt failed: " << strerror(errno) << std::endl;
    }

    freeaddrinfo(res); // Free the linked list

    if (!bound) {
        std::cerr << "Socket::bind error: Could not bind to any address for " << host << ":" << port << std::endl;
        return false;
    }

    return true;
}

bool Socket::listen(int backlog) {
    if (!isValid()) {
        std::cerr << "Socket::listen error: Invalid socket" << std::endl;
        return false;
    }
    if (::listen(_fd, backlog) < 0) {
        std::cerr << "Socket::listen error: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

int Socket::accept(struct sockaddr* addr, socklen_t* addrlen) {
    if (!isValid()) {
        std::cerr << "Socket::accept error: Invalid socket" << std::endl;
        return -1;
    }
    int clientFd = ::accept(_fd, addr, addrlen);
    if (clientFd < 0) {
        // EAGAIN/EWOULDBLOCK are expected for non-blocking sockets, not necessarily errors
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
             std::cerr << "Socket::accept error: " << strerror(errno) << std::endl;
        }
        return -1; // Return -1 on error or no connection pending
    }
    return clientFd;
}

bool Socket::connect(const struct sockaddr* addr, socklen_t addrlen) {
    if (!isValid()) {
        std::cerr << "Socket::connect error: Invalid socket" << std::endl;
        return false;
    }
    if (::connect(_fd, addr, addrlen) < 0) {
        // EINPROGRESS is expected for non-blocking sockets
        if (errno != EINPROGRESS) { 
             std::cerr << "Socket::connect error: " << strerror(errno) << std::endl;
             return false;
        }
        // For non-blocking, connect returns -1 and sets errno to EINPROGRESS.
        // The connection completes asynchronously.
        // The caller needs to use poll/select to check writability for completion.
    }
    return true; // Indicates connection attempt initiated (or succeeded immediately)
}

ssize_t Socket::send(const void* buf, size_t len, int flags) {
    if (!isValid()) {
        std::cerr << "Socket::send error: Invalid socket" << std::endl;
        return -1;
    }
    ssize_t bytesSent = ::send(_fd, buf, len, flags);
    if (bytesSent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
             std::cerr << "Socket::send error: " << strerror(errno) << std::endl;
        }
        // Return -1 on error or if would block
    }
    return bytesSent;
}

ssize_t Socket::recv(void* buf, size_t len, int flags) {
    if (!isValid()) {
        std::cerr << "Socket::recv error: Invalid socket" << std::endl;
        return -1;
    }
    ssize_t bytesRead = ::recv(_fd, buf, len, flags);
    if (bytesRead < 0) {
         if (errno != EAGAIN && errno != EWOULDBLOCK) {
             std::cerr << "Socket::recv error: " << strerror(errno) << std::endl;
         }
         // Return -1 on error or if would block
    } else if (bytesRead == 0) {
        // Peer has closed the connection gracefully
    }
    return bytesRead;
}

bool Socket::close() {
    if (isValid()) {
        if (::close(_fd) < 0) {
            std::cerr << "Socket::close error for fd " << _fd << ": " << strerror(errno) << std::endl;
            // Even if close fails, mark fd as invalid to prevent reuse attempts
            _fd = -1;
            return false;
        }
        _fd = -1;
        return true;
    }
    return true; // Already closed or invalid, considered success
}

// --- Socket Options ---

bool Socket::setReuseAddr(bool enable) {
    if (!isValid()) {
        std::cerr << "Socket::setReuseAddr error: Invalid socket" << std::endl;
        return false;
    }
    int optval = enable ? 1 : 0;
    if (::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Socket::setReuseAddr error: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool Socket::setNonBlocking(bool enable) {
    if (!isValid()) {
        std::cerr << "Socket::setNonBlocking error: Invalid socket" << std::endl;
        return false;
    }
    int flags = ::fcntl(_fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "Socket::setNonBlocking (F_GETFL) error: " << strerror(errno) << std::endl;
        return false;
    }
    flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (::fcntl(_fd, F_SETFL, flags) == -1) {
        std::cerr << "Socket::setNonBlocking (F_SETFL) error: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

// --- Getters ---

int Socket::getFd() const {
    return _fd;
}

bool Socket::isValid() const {
    return _fd >= 0;
}

// --- Release ---

int Socket::release() {
    int tempFd = _fd;
    _fd = -1; // Socket object no longer owns the fd
    return tempFd;
} 