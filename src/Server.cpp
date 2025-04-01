#include "Server.hpp"
#include "Config.hpp" // Include actual headers needed
#include "Socket.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "Client.hpp" // Include Client header
#include <iostream> // Example include
#include <stdexcept> // For runtime_error
#include <unistd.h>  // for close
#include <cstring>   // for memset
#include <fcntl.h> // Include fcntl for client socket
#include <sys/socket.h> // Include socket for accept
#include <netinet/in.h> // Include inet for accept/inet_ntoa
#include <arpa/inet.h> // Include inet_ntoa
#include <sys/stat.h> // For stat()
#include <fstream>   // For ifstream
#include <sstream>   // For stringstream
#include <utility> // For std::piecewise_construct, std::move
#include <tuple>   // For std::forward_as_tuple
#include <cerrno> // For errno
#include <cstdio> // For perror

Server::Server(const Config& config) : _config(config), _epollFd(-1) {
    std::memset(_events, 0, sizeof(_events)); // Clear events buffer
    std::cout << "Server object created." << std::endl;
    // Initialization logic moved to init()
}

Server::~Server() {
    std::cout << "Server object destroying..." << std::endl;
    if (_epollFd >= 0) {
        std::cout << "Closing epoll fd: " << _epollFd << std::endl;
        close(_epollFd);
    }
    // Sockets in _listeningSockets and _clientSockets will be closed
    // by their own destructors when the vectors/maps are cleared.
    std::cout << "Server object destroyed." << std::endl;
}

bool Server::init() {
    try {
        // TODO: Replace with actual config parsing to get ports & hosts
        // For default.conf: listen 127.0.0.1:8080;
        std::vector<std::pair<std::string, int> > listeners;
        listeners.push_back(std::make_pair("127.0.0.1", 8080)); // Use host from config

        // --- Setup based on config ---
        for (size_t i = 0; i < listeners.size(); ++i) {
            const std::string& host = listeners[i].first;
            int port = listeners[i].second;

            std::cout << "Setting up listener on " << host << ":" << port << std::endl;
            Socket listener(port);
            if (!listener.init(host)) { // Pass host to init
                 std::cerr << "Failed to initialize listener socket on " << host << ":" << port << std::endl;
                 return false;
            }
            // Use emplace_back to construct Socket in place (using move constructor)
            _listeningSockets.emplace_back(std::move(listener));
        }
        // --- End Setup ---

        createEpoll();

        // Add all listening sockets to epoll
        for (size_t i = 0; i < _listeningSockets.size(); ++i) {
             addSocketToEpoll(_listeningSockets[i].getFd(), EPOLLIN); // Monitor for incoming connections
             std::cout << "Added listening socket fd=" << _listeningSockets[i].getFd() << " to epoll." << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Server initialization failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

void Server::createEpoll() {
    _epollFd = epoll_create1(0); // Use epoll_create1 for flags if needed (like EPOLL_CLOEXEC)
    if (_epollFd < 0) {
        perror("epoll_create1 failed");
        throw std::runtime_error("Failed to create epoll instance");
    }
    std::cout << "Epoll instance created (fd=" << _epollFd << ")." << std::endl;
}

void Server::addSocketToEpoll(int fd, uint32_t events) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = events; // e.g., EPOLLIN | EPOLLOUT | EPOLLET (Edge Triggered)
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &event) < 0) {
        perror("epoll_ctl(ADD) failed");
        // Consider closing fd or throwing an exception
        throw std::runtime_error("Failed to add socket to epoll");
    }
}

void Server::removeSocketFromEpoll(int fd) {
     if (_epollFd >= 0 && fd >= 0) {
        if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL) < 0) {
            perror("epoll_ctl(DEL) failed");
            // Log error, but might not need to throw, maybe the fd was already closed
        }
     }
}

void Server::run() {
    if (!init()) { // Initialize sockets and epoll before running
        std::cerr << "Server failed to initialize. Aborting." << std::endl;
        return;
    }

    std::cout << "Server running... Waiting for events on epoll fd " << _epollFd << std::endl;

    while (true) { // Main event loop
        int numEvents = epoll_wait(_epollFd, _events, MAX_EVENTS, -1); // Wait indefinitely (-1 timeout)

        if (numEvents < 0) {
            perror("epoll_wait failed");
            // Handle specific errors like EINTR if needed, otherwise maybe break/throw
            if (errno == EINTR) {
                continue; // Interrupted by signal, just restart wait
            }
            // Potentially critical error
             throw std::runtime_error("epoll_wait error");
        }

        // std::cout << "epoll_wait returned " << numEvents << " event(s)." << std::endl;
        handleEpollEvents(numEvents);

        // TODO: Add graceful shutdown logic (e.g., on SIGINT/SIGTERM)
    }
}

void Server::handleEpollEvents(int numEvents) {
    for (int i = 0; i < numEvents; ++i) {
        int fd = _events[i].data.fd;
        uint32_t revents = _events[i].events;

        // Check if the event is on a listening socket
        bool isListener = false;
        for (size_t j = 0; j < _listeningSockets.size(); ++j) {
            if (fd == _listeningSockets[j].getFd()) {
                isListener = true;
                break;
            }
        }

        // Find the client associated with the fd
        std::map<int, Client>::iterator clientIt = _clients.find(fd);

        if (isListener) {
            // Event on a listening socket: incoming connection
             if (revents & EPOLLIN) {
                // std::cout << "Handling new connection on listener fd=" << fd << std::endl;
                handleNewConnection(fd);
            }
             // TODO: Handle listener errors? (EPOLLERR/HUP unlikely but possible)
        } else if (clientIt != _clients.end()) {
            // Event on an existing client connection
            Client& client = clientIt->second;

            if (revents & (EPOLLERR | EPOLLHUP)) {
                // Error or hang-up on client socket
                handleClientError(fd);
            } else {
                if (revents & EPOLLIN) {
                    // Data available to read from client
                    // std::cout << "EPOLLIN event on client fd=" << fd << std::endl;
                    handleClientRead(fd);
                }
                 // Check EPOLLOUT *after* EPOLLIN, as read might trigger a response write
                if (revents & EPOLLOUT) {
                    // Socket is ready for writing
                    // std::cout << "EPOLLOUT event on client fd=" << fd << std::endl;
                    handleClientWrite(fd);
                }
            }

             // Check if client is finished after handling events
             // Close non-keep-alive connections after response sent.
             if (client.getState() == RESPONSE_SENT) {
                  // TODO: Implement Keep-Alive check based on request/response headers
                 bool keepAlive = false; // Default to close for simplicity
                 if (keepAlive) {
                     // client.clear(); // Reset client state
                     // modifySocketInEpoll(fd, EPOLLIN | EPOLLET); // Wait for next request
                     // std::cout << "Client fd=" << fd << ": Keep-Alive - ready for next request." << std::endl;
                 } else {
                     std::cout << "Client fd=" << fd << ": Response sent, closing connection." << std::endl;
                     handleClientDisconnection(fd);
                 }
             }

        } else {
            // Event on an fd that is neither listener nor known client
            // Might happen if client disconnected abruptly before DEL_CTL processed
             std::cerr << "Event on unknown fd=" << fd << ". Removing from epoll." << std::endl;
            removeSocketFromEpoll(fd); // Clean up epoll registration
        }
    }
}

void Server::handleNewConnection(int listenerFd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int clientFd = accept(listenerFd, (struct sockaddr*)&client_addr, &client_len);

    if (clientFd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept failed");
        }
        return;
    }

    // Make the accepted socket non-blocking
    if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl(O_NONBLOCK) for client failed");
        close(clientFd);
        return;
    }

    std::cout << "Accepted new connection (fd=" << clientFd << ") from "
              << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;

    // Add the new client socket to epoll, monitoring for read events initially
    // Use Edge Triggered (EPOLLET) for potentially better performance
    addSocketToEpoll(clientFd, EPOLLIN | EPOLLET);

    // Use emplace with piecewise construction
    _clients.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(clientFd), // Arguments for key (int)
        std::forward_as_tuple(clientFd, client_addr) // Arguments for value (Client)
    );
}

void Server::handleClientRead(int clientFd) {
     std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) return; // Should not happen if called correctly
    Client& client = it->second;

    // Loop reading data because we use Edge Triggering (EPOLLET)
    while (true) {
        ssize_t readResult = client.receiveData(); // Client reads data

        if (readResult == -1) { // Error reported by receiveData
            handleClientDisconnection(clientFd, true);
            return; // Stop processing this client
        } else if (readResult == 0) { // EOF reported by receiveData
            handleClientDisconnection(clientFd);
            return; // Stop processing this client
        } else if (readResult == -2) { // EAGAIN / EWOULDBLOCK reported by receiveData
            // No more data to read right now. Stop the reading loop for this event.
            break;
        } else { // readResult > 0: Data was read
            // Check if the request is complete *after* reading data.
            // Client::receiveData internally sets state to REQUEST_RECEIVED if ready.
            if (client.getState() == REQUEST_RECEIVED) {
                  processRequest(client); // Process the complete request
                  // Note: We might have read part of the *next* request in EPOLLET mode.
                  // processRequest should handle this (e.g., by leaving remaining data
                  // in the buffer for the next EPOLLIN). The current Request::parse
                  // likely consumes only up to the body end.
                  break; // Stop reading loop after processing a request
            }
            // If readResult > 0 but request not yet complete, continue loop
            // (EPOLLET means we must read until EAGAIN/EWOULDBLOCK).
            // The check `readResult < READ_BUFFER_SIZE` is removed,
            // as we rely solely on the return value of receiveData()
            // to know when to stop reading for this EPOLLIN event.
        }
    }
}

void Server::handleClientWrite(int clientFd) {
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end() || it->second.getState() != SENDING_RESPONSE) {
         // Not found or not in a state to send (e.g., already sent, or still reading)
         // If not in SENDING_RESPONSE, maybe remove EPOLLOUT interest?
         // std::cout << "Client fd=" << clientFd << " got EPOLLOUT but not in SENDING_RESPONSE state (" << (it == _clients.end() ? -1 : it->second.getState()) << "). Modifying epoll." << std::endl;
         // modifySocketInEpoll(clientFd, EPOLLIN | EPOLLET); // Revert to only monitoring read
        return;
    }
    Client& client = it->second;

    ssize_t sendResult = client.sendData();

    if (sendResult == -1) { // Error
        handleClientDisconnection(clientFd, true);
    } else if (sendResult == -2) { // EAGAIN / EWOULDBLOCK
        // Kernel buffer is full, need to wait for EPOLLOUT again.
        // Ensure EPOLLOUT is still set (it should be).
        // std::cout << "Client fd=" << clientFd << ": send() would block, waiting for next EPOLLOUT." << std::endl;
    } else if (client.isResponseFullySent()) {
         // If response is fully sent, we might:
         // 1. Close the connection (if not keep-alive) -> handled in handleEpollEvents
         // 2. Switch back to read-only monitoring (if keep-alive)
         // std::cout << "Client fd=" << clientFd << ": Finished sending. Modifying epoll to read-only." << std::endl;
         // modifySocketInEpoll(clientFd, EPOLLIN | EPOLLET); // Switch back to only reading

         // The check and potential disconnection/state reset is done in handleEpollEvents
         // after all events for this iteration are processed.
    }
    // If sendResult > 0 but not fully sent, do nothing; epoll will trigger EPOLLOUT again if needed.
}

void Server::handleClientError(int clientFd) {
    std::cerr << "EPOLLERR/EPOLLHUP detected on client fd=" << clientFd << std::endl;
    handleClientDisconnection(clientFd, true); // Treat as an error disconnection
}

void Server::processRequest(Client& client) {
    client.setState(GENERATING_RESPONSE);
    // std::cout << "Processing request for fd=" << client.getFd() << std::endl;

    // 1. Get the parsed request. getRequest() handles calling Request::parse()
    Request& request = client.getRequest();

    Response response;
    // Use the getter method here
    if (!client.isParsed()) {
        std::cerr << "processRequest called but request not marked as parsed for fd=" << client.getFd() << std::endl;
        response = generateErrorResponse(400, _config); // Bad Request
    } else {
        // 2. Generate Response based on parsed request and config
        // TODO: Pass the actual relevant ServerConfig block
        response = generateResponse(request, _config);
    }


    // 3. Set the response in the client object (this now calls response.toString())
    client.setResponse(response);

    // 4. Modify epoll interest to include EPOLLOUT
    modifySocketInEpoll(client.getFd(), EPOLLIN | EPOLLOUT | EPOLLET);
}

Response Server::generateResponse(const Request& request, const Config& config) {
    // TODO: Get relevant config block based on Host header and port
    // Hardcoded for default.conf for now
    std::string root = "./www/html"; // From default.conf root
    std::vector<std::string> indexFiles;
    indexFiles.push_back("index.html");
    indexFiles.push_back("index.htm");
    // std::string error404page = "/error_pages/404.html"; // Config lookup needed later

    Response response;
    response.setVersion("HTTP/1.1");

    std::cout << "---- generateResponse ----" << std::endl;
    std::cout << "Request Path: " << request.getPath() << std::endl;

    if (request.getMethod() != "GET") {
        std::cout << "-> Method Not Allowed" << std::endl;
        return generateErrorResponse(405, config);
    }

    std::string requestedPath = request.getPath();
    if (requestedPath.find("..") != std::string::npos) {
        std::cout << "-> Directory Traversal Attempt" << std::endl;
        return generateErrorResponse(400, config);
    }
    if (requestedPath.empty() || requestedPath[0] != '/') {
         requestedPath = "/" + requestedPath;
    }

    // Ensure root path does not end with '/' for clean joining, unless root is "/"
    if (root.length() > 1 && root[root.length() - 1] == '/') {
         root.pop_back();
    }
    // Ensure requested path starts with '/' (already done)

    std::string fullPath = root + requestedPath;
    std::cout << "Checking full path: " << fullPath << std::endl;

    struct stat path_stat;
    if (stat(fullPath.c_str(), &path_stat) != 0) {
        // Use perror to print the system error message for stat failure
        perror(("-> stat failed for: " + fullPath).c_str());
        std::cout << "-> Returning 404 (stat failed)" << std::endl;
        return generateErrorResponse(404, config);
    }

    std::string resolvedPath = fullPath; // Path to the actual file/dir

    // If it's a directory
    if (S_ISDIR(path_stat.st_mode)) {
        std::cout << "-> Path is a directory: " << fullPath << std::endl;
        // Ensure directory path ends with '/' for correct index joining
        if (fullPath[fullPath.length() - 1] != '/') {
            fullPath += "/";
            resolvedPath = fullPath; // Update resolved path as well
             std::cout << "   (Appended slash: " << fullPath << ")" << std::endl;
        }

        bool indexFound = false;
        for (size_t i = 0; i < indexFiles.size(); ++i) {
            std::string potentialIndexPath = fullPath + indexFiles[i];
            std::cout << "   Checking for index file: " << potentialIndexPath << std::endl;

            struct stat index_stat;
            if (stat(potentialIndexPath.c_str(), &index_stat) == 0) {
                 std::cout << "   -> stat OK for: " << potentialIndexPath << std::endl;
                if (S_ISREG(index_stat.st_mode)) {
                    std::cout << "   -> Found index file: " << potentialIndexPath << std::endl;
                    resolvedPath = potentialIndexPath;
                    indexFound = true;
                    path_stat = index_stat; // Update stat info to the file's info
                    break;
                } else {
                     std::cout << "   -> Is not a regular file." << std::endl;
                }
            } else {
                 // Don't generate error here, just means this index file doesn't exist
                 // perror(("   -> stat failed for index: " + potentialIndexPath).c_str());
                 std::cout << "   -> Index file not found or stat failed." << std::endl;
            }
        }
        if (!indexFound) {
            // TODO: Implement directory listing based on config (autoindex on;)
            std::cout << "-> No suitable index file found and autoindex off." << std::endl;
            std::cout << "-> Returning 404 (no index)" << std::endl; // Changed from 403
            return generateErrorResponse(404, config);
        }
        // If index found, resolvedPath now points to the index file
         std::cout << "-> Resolved path to index file: " << resolvedPath << std::endl;

    }
    // If it's not a directory AND not a regular file (after potential index resolution)
    else if (!S_ISREG(path_stat.st_mode)) {
         std::cout << "-> Path is not a regular file: " << resolvedPath << std::endl;
         std::cout << "-> Returning 403 (not regular file)" << std::endl;
        return generateErrorResponse(403, config); // Forbidden
    }

    // At this point, resolvedPath points to a valid regular file.
    std::cout << "-> Attempting to serve file: " << resolvedPath << std::endl;
    std::ifstream fileStream(resolvedPath.c_str(), std::ios::binary);
    if (!fileStream.is_open()) {
        // Use errno to understand why opening failed
        std::cerr << "Error: Failed to open file '" << resolvedPath << "': " << strerror(errno) << std::endl;
        std::cout << "-> Returning 500 (cannot open file)" << std::endl;
        return generateErrorResponse(500, config);
    }

    // Determine Content-Type
    std::string contentType = "application/octet-stream";
    size_t dotPos = resolvedPath.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string ext = resolvedPath.substr(dotPos);
        if (ext == ".html" || ext == ".htm") contentType = "text/html";
        else if (ext == ".css") contentType = "text/css";
        else if (ext == ".js") contentType = "application/javascript";
        else if (ext == ".jpg" || ext == ".jpeg") contentType = "image/jpeg";
        else if (ext == ".png") contentType = "image/png";
        else if (ext == ".txt") contentType = "text/plain";
    }
     std::cout << "-> Content-Type: " << contentType << std::endl;

    // Read file content
    std::ostringstream contentStream;
    contentStream << fileStream.rdbuf();
     if (fileStream.fail() && !fileStream.eof()) {
         std::cerr << "Error reading file stream for: " << resolvedPath << std::endl;
         std::cout << "-> Returning 500 (file read error)" << std::endl;
         return generateErrorResponse(500, config);
     }
    std::string body = contentStream.str();
    fileStream.close(); // Close the stream

    std::cout << "-> Read " << body.length() << " bytes from file." << std::endl;

    // Build the 200 OK response
    response.setStatusCode(200);
    response.setHeader("Content-Type", contentType);
    response.setHeader("Content-Length", std::to_string(body.length()));
    response.setHeader("Connection", "close");
    response.setBody(body);

    std::cout << "-> Returning 200 OK" << std::endl;
    std::cout << "--------------------------" << std::endl;
    return response;
}

Response Server::generateErrorResponse(int statusCode, const Config& /*config*/) { // <-- Commented out name
    // TODO: Use config to find custom error pages (e.g., from default.conf error_page 404)
    std::string errorPagePath;
    // if (config.hasErrorPage(statusCode)) { errorPagePath = config.getErrorPage(statusCode); }

    std::string body;
    std::string statusMessage;
    bool customPageFound = false;

    // Simplified status messages
    switch (statusCode) {
        case 400: statusMessage = "Bad Request"; break;
        case 403: statusMessage = "Forbidden"; break;
        case 404: statusMessage = "Not Found"; break;
        case 405: statusMessage = "Method Not Allowed"; break;
        case 500: statusMessage = "Internal Server Error"; break;
        default:  statusMessage = "Error"; statusCode = 500; // Default unknown errors to 500
    }

    // TODO: Try to load custom error page from errorPagePath if set
    // if (!errorPagePath.empty()) { ... try stat/ifstream ... customPageFound = true; }

    if (!customPageFound) {
        // Generate default HTML error page
        std::ostringstream oss;
        oss << "<html><head><title>" << statusCode << " " << statusMessage << "</title></head>"
            << "<body><h1>" << statusCode << " " << statusMessage << "</h1>"
            << "<p>Error processing request.</p><hr><p>webserv</p></body></html>";
        body = oss.str();
    }

    Response response;
    response.setVersion("HTTP/1.1");
    response.setStatusCode(statusCode, statusMessage);
    response.setHeader("Content-Type", "text/html");
    response.setHeader("Content-Length", std::to_string(body.length()));
    response.setHeader("Connection", "close"); // Errors usually close connection
    response.setBody(body);

    std::cerr << "Generated Error Response: " << statusCode << " " << statusMessage << std::endl;

    return response;
}

void Server::handleClientDisconnection(int clientFd, bool isError) {
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) {
        // Already disconnected or never existed
        return;
    }

    if (isError) {
        std::cerr << "Forcing disconnection for client fd=" << clientFd << " due to error." << std::endl;
    } else {
        // std::cout << "Handling disconnection for client fd=" << clientFd << std::endl;
    }

    removeSocketFromEpoll(clientFd); // Remove from epoll interest list
    close(clientFd);                 // Close the socket file descriptor
    _clients.erase(it);              // Remove the Client object from the map
}

// Add helper to modify existing epoll registration
void Server::modifySocketInEpoll(int fd, uint32_t events) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = events; // e.g., EPOLLIN | EPOLLOUT | EPOLLET
    if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, fd, &event) < 0) {
        perror("epoll_ctl(MOD) failed");
        // This is often serious, maybe disconnect client?
        handleClientDisconnection(fd, true); // Treat as error
    }
}

// Implement other Server methods here (setupListeningSockets, etc.)
// void Server::setupListeningSockets() { ... } // Needs config parsing 