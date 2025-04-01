#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <sys/socket.h> // For socket types if needed later
#include <netinet/in.h> // For sockaddr_in
#include "Request.hpp"
#include "Response.hpp"
#include <utility> // For std::move if needed in header later

#define READ_BUFFER_SIZE 4096 // <-- Define it here

enum ClientState {
    AWAITING_REQUEST, // Waiting for/receiving request data
    REQUEST_RECEIVED, // Full request received, ready for processing
    GENERATING_RESPONSE, // Processing request, creating response
    SENDING_RESPONSE,  // Sending response data
    RESPONSE_SENT      // Full response sent (ready for keep-alive or close)
};


class Client {
public:
    Client(int fd, const struct sockaddr_in& addr);
    ~Client();

    // --- Move Semantics ---
    Client(Client&& other) noexcept;            // Move constructor
    Client& operator=(Client&& other) noexcept; // Move assignment operator

    // --- Delete Copy Semantics ---
    Client(const Client&) = delete;            // Explicitly delete copy constructor
    Client& operator=(const Client&) = delete; // Explicitly delete copy assignment

    int getFd() const;
    const struct sockaddr_in& getAddress() const;
    ClientState getState() const;
    void setState(ClientState newState);

    // Request Handling
    ssize_t receiveData(); // Reads data into _requestBuffer
    bool isRequestReady() const; // Checks if full request headers are received
    Request& getRequest(); // Parses if needed, returns Request object
    const std::string& getRawRequest() const; // Get the raw buffer content
    bool isParsed() const; // <-- Add getter for _requestParsed

    // Response Handling
    void setResponse(const Response& response); // Sets the response to be sent
    ssize_t sendData(); // Sends data from _responseBuffer
    bool isResponseFullySent() const;


private:
    int                 _clientFd;
    struct sockaddr_in  _clientAddr;
    ClientState         _state;
    std::string         _requestBuffer; // Buffer for incoming request data
    std::string         _responseBuffer; // Buffer for outgoing response data
    size_t              _bytesSent;     // Track how much of _responseBuffer sent
    Request             _request;       // Parsed request object
    bool                _requestParsed; // Flag to avoid re-parsing


    // Private helper
    void clear(); // Reset client state for reuse (if keep-alive)
};

#endif // CLIENT_HPP 