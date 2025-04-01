#include "Socket.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // for close
#include <fcntl.h>  // for fcntl
#include <stdexcept> // for exceptions
#include <cstring>   // for memset
#include <iostream> // for error messages

// Constructor: Initializes port and sets sockfd to -1
Socket::Socket(int port) : _sockfd(-1), _port(port) {
    std::memset(&_address, 0, sizeof(_address));
    // std::cout << "Socket object created for port " << _port << "." << std::endl;
}

// Destructor: Ensures the socket is closed
Socket::~Socket() {
    // std::cout << "Socket object destroyed." << std::endl;
    closeSocket();
}

// --- Move Constructor ---
Socket::Socket(Socket&& other) noexcept :
    _sockfd(other._sockfd),
    _port(other._port),
    _address(other._address)
{
    // Leave the moved-from object in a safe state (no active socket)
    other._sockfd = -1;
    other._port = 0; // Or keep port? Doesn't matter much if sockfd is -1
    std::memset(&other._address, 0, sizeof(other._address));
     // std::cout << "Socket Move Constructed (fd=" << _sockfd << ")" << std::endl;
}

// --- Move Assignment Operator ---
Socket& Socket::operator=(Socket&& other) noexcept {
    // std::cout << "Socket Move Assigned (target fd=" << _sockfd << ", source fd=" << other._sockfd << ")" << std::endl;
    if (this != &other) { // Prevent self-assignment
        // Close existing socket if necessary before taking ownership of the new one
        closeSocket();

        // Transfer ownership of resources
        _sockfd = other._sockfd;
        _port = other._port;
        _address = other._address;

        // Reset the moved-from object
        other._sockfd = -1;
        other._port = 0;
        std::memset(&other._address, 0, sizeof(other._address));
    }
    return *this;
}

// Initialize the socket: create, set options, bind, listen
bool Socket::init(const std::string& host) {
    // 1. Create socket
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0) {
        perror("socket creation failed");
        return false;
    }
    // std::cout << "Socket created (fd=" << _sockfd << ")." << std::endl;

    // 2. Set socket options (allow address reuse)
    int opt = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        closeSocket();
        return false;
    }
    // std::cout << "Socket option SO_REUSEADDR set." << std::endl;

    // 3. Make socket non-blocking
    if (fcntl(_sockfd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl(O_NONBLOCK) failed");
        closeSocket();
        return false;
    }
    // std::cout << "Socket set to non-blocking." << std::endl;

    // 4. Prepare the sockaddr_in structure
    _address.sin_family = AF_INET;
    // Use inet_addr to convert host string to network address
    _address.sin_addr.s_addr = inet_addr(host.c_str());
    if (_address.sin_addr.s_addr == INADDR_NONE) {
         std::cerr << "Invalid address: " << host << std::endl;
         closeSocket();
         return false;
    }
    _address.sin_port = htons(_port); // Convert port to network byte order

    // 5. Bind the socket to the address and port
    if (bind(_sockfd, (struct sockaddr*)&_address, sizeof(_address)) < 0) {
        perror(("bind failed for " + host + ":" + std::to_string(_port)).c_str());
        closeSocket();
        return false;
    }
    std::cout << "Socket bound to " << host << ":" << _port << "." << std::endl;

    // 6. Listen for incoming connections
    if (listen(_sockfd, SOMAXCONN) < 0) {
        perror("listen failed");
        closeSocket();
        return false;
    }
    std::cout << "Socket listening with backlog " << SOMAXCONN << "." << std::endl;

    return true;
}

// Close the socket
void Socket::closeSocket() {
    if (_sockfd >= 0) {
        // std::cout << "Closing socket (fd=" << _sockfd << ")." << std::endl;
        close(_sockfd);
        _sockfd = -1; // Mark as closed
    }
}

// Get the file descriptor
int Socket::getFd() const {
    return _sockfd;
}
