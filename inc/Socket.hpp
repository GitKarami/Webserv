#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <netinet/in.h> // For sockaddr_in
#include <string> // Include string for host parameter

class Socket {
public:
    // Constructor takes the port
    Socket(int port);
    ~Socket();

    // Initialize the listening socket - now takes host
    bool init(const std::string& host = "127.0.0.1"); // Add host parameter, default localhost
    // Close the socket
    void closeSocket();
    // Get the file descriptor
    int getFd() const;

    // --- Move Semantics ---
    Socket(Socket&& other) noexcept;            // Move constructor
    Socket& operator=(Socket&& other) noexcept; // Move assignment operator

    // --- Delete Copy Semantics ---
    Socket(const Socket&) = delete;            // Explicitly delete copy constructor
    Socket& operator=(const Socket&) = delete; // Explicitly delete copy assignment

    // Add other socket-related methods here if needed later
    // e.g., int acceptConnection(sockaddr_in& client_addr);

private:
    int _sockfd;
    int _port;
    struct sockaddr_in _address;
};

#endif // SOCKET_HPP 