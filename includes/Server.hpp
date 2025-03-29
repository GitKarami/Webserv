#ifndef SERVER_HPP
#define SERVER_HPP

#include "Config.hpp"
#include "Client.hpp"
#include "Socket.hpp"
#include <vector>
#include <map>
#include <memory> // For std::unique_ptr
#include <poll.h> // For poll()
#include <ctime>  // For timeouts
#include <csignal> // For signal handling

class Server {
public:
    Server(const Config& config);
    ~Server();

    // Disable copy/assignment
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool init(); // Setup listening sockets
    void run();  // Start the main server loop
    void stop(); // Signal the server to stop

    // Static signal handler function
    static void signalHandler(int signum);

private:
    const Config& _config; // Reference to the loaded configuration
    std::vector<Socket> _listeningSockets; // Sockets listening for new connections
    std::map<int, std::unique_ptr<Client>> _clients; // Map fd to Client object
    std::vector<struct pollfd> _pollFds; // Structure for poll()

    time_t _clientTimeoutSeconds; // Timeout duration for clients

    // Static flag for signal handling
    static bool _stopServer;

    // Helper methods
    bool setupListeningSockets();
    void updatePollFds();
    void acceptNewConnection(int listeningFd);
    void handleClientRead(int clientFd);
    void handleClientWrite(int clientFd);
    void removeClient(int clientFd, bool forceClose = false);
    void handleTimeouts();

};

#endif // SERVER_HPP 