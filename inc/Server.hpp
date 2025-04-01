#ifndef SERVER_HPP
#define SERVER_HPP

#include "Config.hpp" // Need full definition now
#include "Socket.hpp"
#include "Client.hpp" // Include the new Client header
#include <vector>
#include <map>
#include <sys/epoll.h> // For epoll

#define MAX_EVENTS 10 // Max events to handle at once in epoll_wait

class Server {
public:
    // Constructor takes the fully loaded Config object
    Server(const Config& config);
    ~Server();

    // Initialize server (sockets, epoll)
    bool init();
    // Main server loop
    void run();

private:
    // Configuration
    const Config& _config;

    // Networking
    std::vector<Socket> _listeningSockets; // Store multiple listening sockets
    std::map<int, Client> _clients; // Use std::map<int, Client> to store client state
    int _epollFd;                         // epoll instance file descriptor
    struct epoll_event _events[MAX_EVENTS]; // Buffer for epoll_wait events

    // Private methods for handling server logic
    void setupListeningSockets(); // Create sockets based on config
    void createEpoll();           // Initialize epoll
    void addSocketToEpoll(int fd, uint32_t events);
    void modifySocketInEpoll(int fd, uint32_t events); // Added helper
    void removeSocketFromEpoll(int fd);
    void handleEpollEvents(int numEvents); // Process events from epoll_wait
    void handleNewConnection(int listenerFd); // Accept new client
    void handleClientRead(int clientFd);  // Renamed from handleClientData
    void handleClientWrite(int clientFd); // Added for sending response
    void handleClientError(int clientFd); // Added for EPOLLERR/HUP
    void handleClientDisconnection(int clientFd, bool isError = false); // Updated signature

    // Request/Response Processing
    void processRequest(Client& client); // New method to handle logic
    Response generateResponse(const Request& request, const Config& config); // New method
    Response generateErrorResponse(int statusCode, const Config& config); // New method

    // Prevent copying
    Server(const Server&);
    Server& operator=(const Server&);
};

#endif // SERVER_HPP 