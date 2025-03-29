#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "Socket.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "ServerConfig.hpp" // To access config details for request handling
#include <memory> // For std::unique_ptr
#include <vector>
#include <ctime> // For timeout tracking

class Client {
public:
    // Enum for read/write results (can be expanded)
    enum IoResult {
        Success,    // Operation successful, potentially more needed
        Complete,   // Operation completed the task (e.g., response sent)
        Closed,     // Connection closed by peer
        Error       // An error occurred
    };

    // Constructor takes ownership of the client socket fd and the relevant server config
    Client(int clientFd, const ServerConfig* config);
    ~Client();

    // Disable copy
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // Getters
    int getFd() const;
    const ServerConfig* getConfig() const;
    bool isReadyToSend() const; // Does the response have data ready to be sent?
    bool isMarkedForRemoval() const; // Should this client be removed?

    // I/O Handlers called by Server
    IoResult handleRead();
    IoResult handleWrite();

    // Check for timeouts
    bool checkTimeout(time_t currentTime, time_t timeoutSeconds);

private:
    Socket _socket; // RAII wrapper for the client socket fd
    const ServerConfig* _config; // Pointer to the relevant server configuration
    Request _request;
    Response _response;

    bool _markedForRemoval;
    time_t _lastActivityTime;

    // Internal state/logic helpers
    void processRequest(); // Called when request parsing is complete
    void buildErrorResponse(int statusCode);
    // Add CGI handling state/methods if needed
    // Add Upload handling state/methods if needed

    void prepareResponseToSend(); // Calls _response.buildResponse()
};

#endif // CLIENT_HPP 