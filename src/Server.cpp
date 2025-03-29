#include "Server.hpp"
#include "Utils.hpp"
#include <iostream>
#include <sys/socket.h> // For socket, bind, listen, accept
#include <netinet/in.h> // For sockaddr_in, INADDR_ANY
#include <arpa/inet.h> // For inet_pton
#include <unistd.h>     // For close (handled by Socket RAII)
#include <fcntl.h>      // For fcntl, O_NONBLOCK
#include <poll.h>       // For poll
#include <csignal>      // For signal
#include <cstring>      // For strerror, memset
#include <stdexcept>    // For runtime_error
#include <algorithm>    // For std::find_if
#include <set>          // To avoid duplicate listen addresses
#include <memory>       // For std::make_unique

// Initialize static member
bool Server::_stopServer = false;

// Static signal handler
void Server::signalHandler(int signum) {
    std::cout << "\nCaught signal " << signum << ", shutting down..." << std::endl;
    _stopServer = true;
}

Server::Server(const Config& config)
    : _config(config), _clientTimeoutSeconds(60) { // Default timeout 60s
}

Server::~Server() {
    // Clients are managed by unique_ptr, sockets by RAII
    std::cout << "Server shutting down." << std::endl;
}

bool Server::init() {
    std::cout << "Initializing server..." << std::endl;
    if (!setupListeningSockets()) {
        return false;
    }

    // Setup signal handlers
    signal(SIGINT, Server::signalHandler);
    signal(SIGTERM, Server::signalHandler);
    // Ignore SIGPIPE to prevent crashing on write to closed socket (handle EPIPE instead)
    signal(SIGPIPE, SIG_IGN);

    std::cout << "Server initialized successfully. Listening on configured ports." << std::endl;
    return true;
}

bool Server::setupListeningSockets() {
    const auto& serverConfigs = _config.getServerConfigs();
    if (serverConfigs.empty()) {
        std::cerr << "Error: No server configurations found." << std::endl;
        return false;
    }

    // Use a set to track unique host:port combinations to avoid duplicate binds
    std::set<std::pair<std::string, int>> boundAddresses;

    for (const auto& conf : serverConfigs) {
        std::pair<std::string, int> addr = {conf.host, conf.port};
        if (boundAddresses.count(addr)) {
            // std::cout << "Skipping duplicate listen address: " << conf.host << ":" << conf.port << std::endl;
            continue; // Already bound this address
        }

        try {
            Socket listenSocket;
            if (!listenSocket.create()) throw std::runtime_error("Socket creation failed");
            if (!listenSocket.setReuseAddr()) throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
            if (!listenSocket.bind(conf.host, conf.port)) throw std::runtime_error("Socket bind failed");
            if (!listenSocket.listen(SOMAXCONN)) throw std::runtime_error("Socket listen failed"); // Use system max backlog
            if (!listenSocket.setNonBlocking()) throw std::runtime_error("Setting non-blocking failed");

            std::cout << "Listening on " << conf.host << ":" << conf.port << " (fd: " << listenSocket.getFd() << ")" << std::endl;
            _listeningSockets.push_back(std::move(listenSocket)); // Move socket into vector
            boundAddresses.insert(addr);
        } catch (const std::runtime_error& e) {
            std::cerr << "Error setting up listener for " << conf.host << ":" << conf.port << " - " << e.what() << std::endl;
            // Should we continue trying other configs or fail completely?
            // For now, let's fail completely if any listener fails.
            return false;
        }
    }

    if (_listeningSockets.empty()) {
        std::cerr << "Error: No listening sockets were successfully created." << std::endl;
        return false;
    }

    return true;
}

void Server::stop() {
    _stopServer = true;
}

void Server::run() {
    std::cout << "Server starting run loop..." << std::endl;
    int pollTimeoutMs = 1000; // Poll timeout 1 second

    while (!_stopServer) {
        updatePollFds();

        int pollCount = poll(_pollFds.data(), _pollFds.size(), pollTimeoutMs);

        if (_stopServer) break; // Check again after poll returns

        if (pollCount < 0) {
            // EINTR is expected if a signal interrupted poll, continue normally
            if (errno == EINTR) continue;
            perror("poll error");
            break; // Fatal error
        }

        // Handle timeouts regardless of poll result (it might have timed out)
        handleTimeouts();

        if (pollCount == 0) {
            // Timeout occurred, already handled by handleTimeouts() above
            continue;
        }

        // Process events - make copy of fds to iterate over as main list might change
        std::vector<struct pollfd> currentFds = _pollFds;
        std::vector<int> clientsToRemove; // Collect fds to remove after iteration

        // Use C++11 index-based loop for poll results vector
        for (size_t i = 0; i < currentFds.size(); ++i) {
            const struct pollfd& pfd = currentFds[i];

            if (pfd.revents == 0) continue; // No events for this fd

            // --- DEBUG LOGGING --- START
            std::cout << "[DEBUG] Event on fd=" << pfd.fd << " revents=" << pfd.revents;
            if (pfd.revents & POLLIN) std::cout << " POLLIN";
            if (pfd.revents & POLLOUT) std::cout << " POLLOUT";
            if (pfd.revents & POLLERR) std::cout << " POLLERR";
            if (pfd.revents & POLLHUP) std::cout << " POLLHUP";
            if (pfd.revents & POLLNVAL) std::cout << " POLLNVAL";
            std::cout << std::endl;
            // --- DEBUG LOGGING --- END

            bool isListener = false;
            for(size_t j = 0; j < _listeningSockets.size(); ++j) {
                if (pfd.fd == _listeningSockets[j].getFd()) {
                    isListener = true;
                    break;
                }
            }

            if (isListener) {
                // Handle listener socket event
                if (pfd.revents & POLLIN) {
                    acceptNewConnection(pfd.fd);
                }
                // Handle other listener events? (Errors?)
                 if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                     std::cerr << "Error on listening socket fd=" << pfd.fd << ". Shutting down?" << std::endl;
                      _stopServer = true; // Stop server on listener error
                 }
            } else {
                // Handle client socket event
                int clientFd = pfd.fd;
                if (_clients.find(clientFd) == _clients.end()) {
                     continue;
                }

                 // Handle errors first
                if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    clientsToRemove.push_back(clientFd);
                    continue; // Don't process other events if error occurred
                }

                // Handle read if requested and no error
                 if (pfd.revents & POLLIN) {
                     handleClientRead(clientFd);
                     if (_clients.count(clientFd) && _clients.at(clientFd)->isMarkedForRemoval()) {
                         bool found = false;
                         for(size_t k=0; k < clientsToRemove.size(); ++k) if(clientsToRemove[k] == clientFd) found = true;
                         if(!found) clientsToRemove.push_back(clientFd);
                         continue; // Skip write check if read caused removal
                     }
                 }

                // Handle write if requested, no error, and not marked for removal by read
                if (pfd.revents & POLLOUT) {
                    handleClientWrite(clientFd);
                    if (_clients.count(clientFd) && _clients.at(clientFd)->isMarkedForRemoval()) {
                        bool found = false;
                        for(size_t k=0; k < clientsToRemove.size(); ++k) if(clientsToRemove[k] == clientFd) found = true;
                        if(!found) clientsToRemove.push_back(clientFd);
                    }
                }
            }
         } // End of iterating through poll results

        // Remove clients marked for deletion
        for (size_t i = 0; i < clientsToRemove.size(); ++i) {
            removeClient(clientsToRemove[i]);
        }

    } // End while(!_stopServer)

    std::cout << "Server run loop finished." << std::endl;
}

void Server::updatePollFds() {
    _pollFds.clear();

    // Add listening sockets
    for (size_t i = 0; i < _listeningSockets.size(); ++i) {
        const Socket& socket = _listeningSockets[i];
        struct pollfd pfd = {};
        pfd.fd = socket.getFd();
        pfd.events = POLLIN; // Only interested in reading (accepting)
        pfd.revents = 0;
        _pollFds.push_back(pfd);
    }

    // Add client sockets
    // Use C++11 iterator loop
    for (std::map<int, std::unique_ptr<Client> >::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        const std::unique_ptr<Client>& client = it->second;
        if (!client || client->isMarkedForRemoval()) continue; // Don't poll clients marked for removal or invalid

        struct pollfd pfd = {};
        pfd.fd = client->getFd();
        pfd.events = POLLIN; // Always interested in reading from client
        if (client->isReadyToSend()) {
            pfd.events |= POLLOUT; // Interested in writing if response is ready
        }
        pfd.revents = 0;
        _pollFds.push_back(pfd);
    }
}

void Server::acceptNewConnection(int listeningFd) {
    while (true) { // Loop to accept all pending connections on this listener
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientFd = accept(listeningFd, (struct sockaddr*)&clientAddr, &clientAddrLen);

        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // No more connections to accept now
            } else {
                perror("accept error");
                break; // Stop trying to accept on this listener for now
            }
        }

        // Successfully accepted a connection
        const ServerConfig* matchedConfig = NULL;
        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if (getsockname(listeningFd, (struct sockaddr *)&sin, &len) == 0) {
            int listenerPort = ntohs(sin.sin_port);
            const std::vector<ServerConfig>& serverConfigs = _config.getServerConfigs();
            for (size_t i = 0; i < serverConfigs.size(); ++i) {
                if (serverConfigs[i].port == listenerPort) {
                    matchedConfig = &serverConfigs[i];
                    break;
                }
            }
        } else {
            perror("getsockname error");
            close(clientFd);
            continue; // Try next accept() if loop continues, or break
        }

        if (!matchedConfig) {
             std::cerr << "Error: Could not find ServerConfig for new connection on fd=" << listeningFd << " (port " << (sin.sin_family == AF_INET ? ntohs(sin.sin_port) : -1) << ")" << std::endl;
             close(clientFd); // Close the accepted socket
             continue; // Continue accept() loop
        }

        try {
            // Create client using new with unique_ptr (C++11)
            std::unique_ptr<Client> newClient(new Client(clientFd, matchedConfig));
            if (newClient->isMarkedForRemoval()) { // Check if non-blocking setup failed in constructor
                std::cerr << "Failed to initialize new client on fd=" << clientFd << std::endl;
            } else {
                _clients[clientFd] = std::move(newClient); // Transfer ownership to map
            }
        } catch (const std::bad_alloc& e) {
            std::cerr << "Error: Memory allocation failed for new client." << std::endl;
            close(clientFd);
        } catch (const std::exception& e) {
            std::cerr << "Error creating new client: " << e.what() << std::endl;
            close(clientFd);
        } catch (...) {
            std::cerr << "Unknown error creating new client for fd=" << clientFd << std::endl;
            close(clientFd);
        }
     } // End while(true) for accept
}

void Server::handleClientRead(int clientFd) {
    std::map<int, std::unique_ptr<Client> >::iterator it = _clients.find(clientFd);
    if (it == _clients.end() || !it->second) return; // Client might have been removed already

    Client* client = it->second.get();
    Client::IoResult result = client->handleRead();
    (void)result; // Suppress unused variable warning if not used
}

void Server::handleClientWrite(int clientFd) {
    std::map<int, std::unique_ptr<Client> >::iterator it = _clients.find(clientFd);
     if (it == _clients.end() || !it->second) return; // Client removed

    Client* client = it->second.get();
     if (!client->isReadyToSend()) {
         return;
     }

    Client::IoResult result = client->handleWrite();
    (void)result; // Suppress unused variable warning if not used
}

void Server::removeClient(int clientFd, bool /*forceClose*/) {
    std::map<int, std::unique_ptr<Client> >::iterator it = _clients.find(clientFd);
    if (it != _clients.end()) {
        _clients.erase(it);
    } else {
        // std::cerr << "Attempted to remove client fd=" << clientFd << " which was already removed." << std::endl;
    }
}

void Server::handleTimeouts() {
    time_t currentTime = std::time(NULL);
    std::vector<int> timedOutClients;

    // Use C++11 iterator loop
    for (std::map<int, std::unique_ptr<Client> >::iterator it = _clients.begin(); it != _clients.end(); ++it) {
         const std::unique_ptr<Client>& client = it->second;
         // Check if client pointer is valid before using
        if (client && client->checkTimeout(currentTime, _clientTimeoutSeconds)) {
             timedOutClients.push_back(it->first);
         }
     }

    for (size_t i = 0; i < timedOutClients.size(); ++i) {
        removeClient(timedOutClients[i]);
    }
} 