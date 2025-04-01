#include "Client.hpp"
#include <unistd.h> // for close, read, write
#include <iostream>
#include <vector>
#include <sys/socket.h> // for recv, send
#include <cstring> // for strerror
#include <cerrno> // for errno
#include <utility> // For std::move

// #define READ_BUFFER_SIZE 4096 // <-- Remove definition from here

Client::Client(int fd, const struct sockaddr_in& addr) :
    _clientFd(fd),
    _clientAddr(addr),
    _state(AWAITING_REQUEST),
    _bytesSent(0),
    _requestParsed(false)
{
    // std::cout << "Client created for fd=" << _clientFd << std::endl;
}

Client::~Client() {
    // std::cout << "Client destroyed for fd=" << _clientFd << std::endl;
    if (_clientFd >= 0) {
        // Closing is handled by Server::handleClientDisconnection to ensure epoll removal
        // close(_clientFd);
    }
}

void Client::clear() {
     _requestBuffer.clear();
     _responseBuffer.clear();
     _request = Request(); // Reset request object
     _requestParsed = false;
     _bytesSent = 0;
     _state = AWAITING_REQUEST;
     // Keep _clientFd and _clientAddr
}


int Client::getFd() const {
    return _clientFd;
}

const struct sockaddr_in& Client::getAddress() const {
    return _clientAddr;
}

ClientState Client::getState() const {
    return _state;
}

void Client::setState(ClientState newState) {
    // std::cout << "Client fd=" << _clientFd << " state changed to " << newState << std::endl;
    _state = newState;
}

const std::string& Client::getRawRequest() const {
    return _requestBuffer;
}

bool Client::isParsed() const {
    return _requestParsed;
}

// Reads data from socket into _requestBuffer
// Returns: bytes read, 0 on EOF, -1 on error, -2 on EAGAIN/EWOULDBLOCK
ssize_t Client::receiveData() {
    std::vector<char> buffer(READ_BUFFER_SIZE);
    ssize_t bytes_read = recv(_clientFd, buffer.data(), buffer.size(), 0);

    if (bytes_read > 0) {
        _requestBuffer.append(buffer.data(), bytes_read);
        // Check if headers are complete after receiving new data
        if (isRequestReady() && _state == AWAITING_REQUEST) {
            _state = REQUEST_RECEIVED;
            std::cout << "Client fd=" << _clientFd << ": Request received." << std::endl;
        }
    } else if (bytes_read == 0) {
        // Connection closed by peer
        std::cout << "Client fd=" << _clientFd << ": Connection closed by peer." << std::endl;
        _state = RESPONSE_SENT; // Treat as finished
        return 0; // Indicate EOF
    } else { // bytes_read < 0
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
             // No more data available right now (non-blocking)
            return -2; // Indicate non-blocking would block
        } else {
            // Actual error
            perror("recv failed");
            _state = RESPONSE_SENT; // Treat as finished/error
            return -1; // Indicate error
        }
    }
    return bytes_read;
}

// Check if the request headers seem complete (contains "\r\n\r\n")
bool Client::isRequestReady() const {
    return _requestBuffer.find("\r\n\r\n") != std::string::npos;
}

// Get the parsed request object
Request& Client::getRequest() {
    if (!_requestParsed && isRequestReady()) {
        try {
            // Call the actual parsing function
            if (_request.parse(_requestBuffer)) {
                 _requestParsed = true;
                 std::cout << "Client fd=" << _clientFd << ": Request parsed successfully." << std::endl;
            } else {
                 // Parsing failed (e.g., bad syntax, incomplete body needed)
                 // If parse returns false because more body data is needed, keep _requestParsed = false
                 // If parse returns false due to syntax error, maybe throw or set error state?
                 // For now, assume false means syntax error or unrecoverable issue.
                 std::cerr << "Client fd=" << _clientFd << ": Request::parse returned false." << std::endl;
                  _requestParsed = true; // Prevent retry loop on syntax error
                  _state = GENERATING_RESPONSE; // Generate 400 Bad Request
                  // Potentially clear the request object or set error flag in it?
            }
        } catch (const std::exception& e) {
             std::cerr << "Client fd=" << _clientFd << ": Request parsing exception: " << e.what() << std::endl;
             _requestParsed = true; // Mark as parsed even on error to avoid loop
             _state = GENERATING_RESPONSE; // Move to generate error response
        }
    }
    return _request;
}


// Store the generated response string
void Client::setResponse(const Response& response) {
    // Use Response::toString() to generate the full response string
    _responseBuffer = response.toString();
    _bytesSent = 0;
    setState(SENDING_RESPONSE);
    std::cout << "Client fd=" << _clientFd << ": Response set (" << _responseBuffer.length() << " bytes)." << std::endl;
     // Optional: Log response headers for debugging
     // size_t headers_end = _responseBuffer.find("\r\n\r\n");
     // if (headers_end != std::string::npos) {
     //      std::cout << "--- Response Headers fd=" << _clientFd << " ---\n"
     //                << _responseBuffer.substr(0, headers_end) << "\n------------------------" << std::endl;
     // }
}

// Send data from _responseBuffer
// Returns: bytes sent, 0 if nothing to send, -1 on error, -2 on EAGAIN/EWOULDBLOCK
ssize_t Client::sendData() {
    if (_responseBuffer.empty() || _bytesSent >= _responseBuffer.length()) {
        return 0; // Nothing (more) to send
    }

    size_t bytes_to_send = _responseBuffer.length() - _bytesSent;
    const char* buffer_ptr = _responseBuffer.c_str() + _bytesSent;

    ssize_t bytes_written = send(_clientFd, buffer_ptr, bytes_to_send, 0); // Consider MSG_NOSIGNAL

    if (bytes_written > 0) {
        _bytesSent += bytes_written;
        // std::cout << "Client fd=" << _clientFd << ": Sent " << bytes_written << " bytes (" << _bytesSent << "/" << _responseBuffer.length() << ")" << std::endl;
        if (isResponseFullySent()) {
            std::cout << "Client fd=" << _clientFd << ": Full response sent." << std::endl;
            setState(RESPONSE_SENT);
            // If keep-alive: clear(); and set state AWAITING_REQUEST
            // If not keep-alive: leave state as RESPONSE_SENT for server to close
        }
        return bytes_written;
    } else if (bytes_written == 0) {
        // Should not happen with TCP unless bytes_to_send was 0
        std::cerr << "Client fd=" << _clientFd << ": send() returned 0." << std::endl;
        return 0;
    } else { // bytes_written < 0
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Socket buffer is full, try again later
             std::cout << "Client fd=" << _clientFd << ": send() would block (EAGAIN/EWOULDBLOCK)." << std::endl;
            return -2; // Indicate non-blocking would block
        } else if (errno == EPIPE) {
             // Client closed connection unexpectedly (broken pipe)
            std::cerr << "Client fd=" << _clientFd << ": send() failed (Broken pipe)." << std::endl;
             setState(RESPONSE_SENT); // Mark as done/error
             return -1;
        }
         else {
            // Other error
            perror("send failed");
            setState(RESPONSE_SENT); // Mark as done/error
            return -1; // Indicate error
        }
    }
}

bool Client::isResponseFullySent() const {
    return _bytesSent == _responseBuffer.length() && !_responseBuffer.empty();
}

// --- Move Constructor ---
Client::Client(Client&& other) noexcept :
    _clientFd(other._clientFd),
    _clientAddr(other._clientAddr), // sockaddr_in is trivially copyable
    _state(other._state),
    _requestBuffer(std::move(other._requestBuffer)), // Move strings
    _responseBuffer(std::move(other._responseBuffer)),
    _bytesSent(other._bytesSent),
    _request(std::move(other._request)), // Assuming Request is movable
    _requestParsed(other._requestParsed)
{
    // Leave the moved-from object in a defined (but unusable for socket ops) state
    other._clientFd = -1; // Mark fd as invalid in the source
    other._state = AWAITING_REQUEST; // Or some other safe state
    other._bytesSent = 0;
    other._requestParsed = false;
    // std::cout << "Client Move Constructed (fd=" << _clientFd << ")" << std::endl;
}

// --- Move Assignment Operator ---
Client& Client::operator=(Client&& other) noexcept {
    // std::cout << "Client Move Assigned (target fd=" << _clientFd << ", source fd=" << other._clientFd << ")" << std::endl;
    if (this != &other) {
        // Note: We don't close the current _clientFd here, as its lifetime
        // is managed by the Server map/epoll. Just transfer state.

        _clientFd = other._clientFd;
        _clientAddr = other._clientAddr;
        _state = other._state;
        _requestBuffer = std::move(other._requestBuffer);
        _responseBuffer = std::move(other._responseBuffer);
        _bytesSent = other._bytesSent;
        _request = std::move(other._request); // Assuming Request is movable
        _requestParsed = other._requestParsed;

        // Reset the moved-from object
        other._clientFd = -1;
        other._state = AWAITING_REQUEST;
        other._bytesSent = 0;
        other._requestParsed = false;
        other._requestBuffer.clear(); // Clear strings
        other._responseBuffer.clear();
    }
    return *this;
} 