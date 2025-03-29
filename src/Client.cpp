#include "Client.hpp"
#include "Utils.hpp" // For error messages, potentially
#include <iostream>   // For cerr
#include <sys/socket.h> // For recv, send
#include <unistd.h>     // For close (handled by Socket RAII), fcntl
#include <fcntl.h>      // For fcntl, O_NONBLOCK
#include <vector>

// Constructor
Client::Client(int clientFd, const ServerConfig* config)
    : _socket(clientFd), _config(config), _markedForRemoval(false) {
    // Set socket to non-blocking
    if (!_socket.setNonBlocking()) {
        std::cerr << "Error setting client socket to non-blocking." << std::endl;
        _markedForRemoval = true; // Mark for removal if setup fails
        // _socket destructor will handle closing the fd
    }
    _lastActivityTime = std::time(nullptr); // Initialize last activity time
    // std::cout << "Client connected: fd=" << getFd() << std::endl; // Debug
}

// Destructor
Client::~Client() {
    // std::cout << "Client disconnected: fd=" << getFd() << std::endl; // Debug
    // Socket RAII handles closing the fd
}

// --- Getters ---
int Client::getFd() const {
    return _socket.getFd();
}

const ServerConfig* Client::getConfig() const {
    return _config;
}

// Check if response is built and ready to be sent (not fully sent yet)
bool Client::isReadyToSend() const {
    return !_response.isComplete() && _response.getRawResponsePtr() != nullptr;
}

bool Client::isMarkedForRemoval() const {
    return _markedForRemoval;
}

// --- I/O Handlers ---

Client::IoResult Client::handleRead() {
    if (_markedForRemoval) return Error;

    char buffer[4096]; // Read buffer
    ssize_t bytesRead;

    // --- DEBUG LOGGING ---
    std::cout << "[DEBUG Client fd=" << getFd() << "] Entering handleRead" << std::endl;

    bytesRead = recv(getFd(), buffer, sizeof(buffer), 0);
    _lastActivityTime = std::time(nullptr);

    // --- DEBUG LOGGING ---
    std::cout << "[DEBUG Client fd=" << getFd() << "] recv returned: " << bytesRead;
    if (bytesRead < 0) std::cout << " (errno: " << errno << ")";
    std::cout << std::endl;

    if (bytesRead == 0) {
        // Connection closed by peer
        std::cout << "[DEBUG Client fd=" << getFd() << "] Connection closed by peer." << std::endl;
        _markedForRemoval = true;
        return Closed;
    } else if (bytesRead < 0) {
        // Error or non-blocking would block
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // --- DEBUG LOGGING ---
            std::cout << "[DEBUG Client fd=" << getFd() << "] recv would block (EAGAIN/EWOULDBLOCK)." << std::endl;
            return Success; // Nothing to read right now
        } else {
            perror("recv error");
            std::cerr << "[DEBUG Client fd=" << getFd() << "] recv error." << std::endl;
            _markedForRemoval = true;
            return Error;
        }
    } else {
        // Data received
        std::cout << "[DEBUG Client fd=" << getFd() << "] Received " << bytesRead << " bytes." << std::endl;

        // Pass received data directly to the parser
        Request::ParseState parseStatus = _request.parse(buffer, bytesRead);

        // --- DEBUG LOGGING ---
        std::cout << "[DEBUG Client fd=" << getFd() << "] Request::parse returned state: " << parseStatus << std::endl;

        if (parseStatus == Request::Complete) {
            std::cout << "[DEBUG Client fd=" << getFd() << "] Request complete, processing..." << std::endl;
            processRequest(); // Request fully parsed, process it
            return Success;   // Ready for potential write
        } else if (parseStatus == Request::Error) {
            int errorCode = _request.getErrorCode();
            std::cerr << "[DEBUG Client fd=" << getFd() << "] Request parsing error (Code: " << errorCode << ")" << std::endl;
            buildErrorResponse(errorCode); // Build appropriate error response (400, 413, 505 etc.)
            return Success; // Ready to send error response
        } else {
            // Parsing incomplete (state is something like ParsingHeaders, ParsingBody etc.)
            std::cout << "[DEBUG Client fd=" << getFd() << "] Request parsing incomplete." << std::endl;
            return Success; // Need more data
        }
    }
}

Client::IoResult Client::handleWrite() {
    if (_markedForRemoval) return Error; // Already marked for removal, don't try to write
    // --- DEBUG LOGGING ---
    std::cout << "[DEBUG Client fd=" << getFd() << "] Entering handleWrite. isReadyToSend()=" << isReadyToSend() << std::endl;

    if (!isReadyToSend()) return Success; // Nothing ready/pending to send

    const char* dataToSend = _response.getRawResponsePtr();
    // Ensure dataSize calculation uses the remaining bytes
    size_t remainingSize = _response.getRawResponseSize() - _response.getBytesSent();

    // --- DEBUG LOGGING ---
    std::cout << "[DEBUG Client fd=" << getFd() << "] Attempting to send " << remainingSize << " bytes." << std::endl;

    if (!dataToSend || remainingSize == 0) {
         // This state should ideally be covered by isReadyToSend(), but double-check.
        // --- DEBUG LOGGING ---
        std::cerr << "[DEBUG Client fd=" << getFd() << "] handleWrite called but no data available (ptr=" 
                  << (void*)dataToSend << ", remaining=" << remainingSize 
                  << ", isComplete=" << _response.isComplete() << "). Marking for removal." << std::endl;
        _markedForRemoval = true; // Mark for removal due to inconsistency
        return Error;
    }


    ssize_t bytesSent = send(getFd(), dataToSend, remainingSize, 0);
    _lastActivityTime = std::time(nullptr);

    // --- DEBUG LOGGING ---
    std::cout << "[DEBUG Client fd=" << getFd() << "] send returned: " << bytesSent;
    if (bytesSent < 0) std::cout << " (errno: " << errno << ")";
    std::cout << std::endl;

    if (bytesSent < 0) {
        // Error or non-blocking would block
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::cout << "[DEBUG Client fd=" << getFd() << "] send would block (EAGAIN/EWOULDBLOCK)." << std::endl;
            return Success; // Cannot write right now, try again later
        } else if (errno == EPIPE) { // Broken pipe (client closed connection)
            std::cout << "[DEBUG Client fd=" << getFd() << "] send failed (EPIPE), marking for removal." << std::endl;
            _markedForRemoval = true;
            return Closed;
        } else {
            perror("send error");
            std::cerr << "[DEBUG Client fd=" << getFd() << "] send error, marking for removal." << std::endl;
            _markedForRemoval = true;
            return Error;
        }
    } else {
        // Data sent successfully
        std::cout << "[DEBUG Client fd=" << getFd() << "] Sent " << bytesSent << " bytes." << std::endl;
        _response.bytesSent(static_cast<size_t>(bytesSent));
        if (_response.isComplete()) {
            std::cout << "[DEBUG Client fd=" << getFd() << "] Response completely sent." << std::endl;
            // Response is fully sent. Handle based on Connection header (or default).
            std::string connectionHeader = _request.getHeader("Connection"); // Assuming Request has getHeader
            std::cout << "[DEBUG Client fd=" << getFd() << "] Connection header: '" << connectionHeader << "'" << std::endl;
             // Normalize connectionHeader if necessary (e.g., to lower case)
            if (connectionHeader.find("keep-alive") != std::string::npos) {
                 std::cout << "[DEBUG Client fd=" << getFd() << "] Keep-alive requested. Resetting client state." << std::endl;
                 // Reset for the next request
                 _request.clear();
                 _response.clear();
                 // Don't mark for removal, wait for next read event.
                return Complete; // Indicate the *current response* is complete
            } else {
                 std::cout << "[DEBUG Client fd=" << getFd() << "] Connection: close or header absent. Marking for removal." << std::endl;
                 // Default or "Connection: close"
                 _markedForRemoval = true; // Mark for removal after sending
                 return Complete; // Indicate response complete, connection will close.
             }

        } else {
            // More data remains to be sent
            std::cout << "[DEBUG Client fd=" << getFd() << "] More data to send (" << (_response.getRawResponseSize() - _response.getBytesSent()) << " remaining)." << std::endl;
            return Success;
        }
    }
}

// --- Timeout Check ---
bool Client::checkTimeout(time_t currentTime, time_t timeoutSeconds) {
    if (difftime(currentTime, _lastActivityTime) > timeoutSeconds) {
        // std::cout << "Client timed out: fd=" << getFd() << std::endl;
        _markedForRemoval = true;
        return true;
    }
    return false;
}

// --- Internal Logic ---

// Placeholder: Process a fully parsed request
void Client::processRequest() {
    // TODO: Implement actual request processing logic here
    // - Find matching location block using _config->findLocation(_request.getPath())
    // - Check allowed methods in the location
    // - Handle GET (serve static file, directory listing, CGI), POST (CGI, upload), DELETE
    // - Handle redirection
    // - Handle CGI execution
    // - Handle file uploads

    // Example: Simple placeholder response
    std::string body = "<html><body><h1>Hello from Webserv!</h1><p>Request received for path: " +
                       _request.getPath() + "</p></body></html>";
    _response.clear(); // Reset response state
    _response.setStatusCode(200);
    _response.setHttpVersion(_request.getHttpVersion()); // Use the request's HTTP version
    _response.setHeader("Content-Type", "text/html");
    _response.setBody(body); // Sets body and potentially Content-Length

    prepareResponseToSend();
}

// Placeholder: Build an error response
void Client::buildErrorResponse(int statusCode) {
    _response.clear(); // Reset any previous response state
    _response.setStatusCode(statusCode);
    _response.setHttpVersion(_request.getHttpVersion().empty() ? "HTTP/1.1" : _request.getHttpVersion()); // Use request version or default

    // TODO: Check config for custom error pages
    // std::map<int, std::string>::const_iterator it = _config->errorPages.find(statusCode);
    // if (it != _config->errorPages.end()) { ... load error page ... }

    // Default error page
    std::string errorBody = "<html><head><title>" + std::to_string(statusCode) + " " +
                            Utils::getHttpStatusMessage(statusCode) + "</title></head><body><h1>" +
                            std::to_string(statusCode) + " " + Utils::getHttpStatusMessage(statusCode) +
                            "</h1></body></html>";

    _response.setHeader("Content-Type", "text/html");
    _response.setBody(errorBody);

    // Ensure Connection: close for error responses? Usually a good idea.
    _response.setHeader("Connection", "close");

    prepareResponseToSend();
}

// Prepare the response to be sent over the socket
void Client::prepareResponseToSend() {
    _response.buildResponse();
    // std::cout << "Prepared response for fd=" << getFd() << ":\n" << std::string(_response.getRawResponsePtr(), _response.getRawResponseSize()) << std::endl; // Debug
} 