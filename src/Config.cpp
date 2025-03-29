#include "Config.hpp"
#include "Utils.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib> // For strtol
#include <limits>  // For numeric_limits

Config::Config() : _parsePos(0) {}

Config::~Config() {}

const std::vector<ServerConfig>& Config::getServerConfigs() const {
    return _servers;
}

// --- File Loading ---

bool Config::loadFromFile(const std::string& filename) {
    std::ifstream configFile(filename.c_str());
    if (!configFile.is_open()) {
        std::cerr << "Error: Could not open configuration file: " << filename << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << configFile.rdbuf();
    _configContent = buffer.str();
    configFile.close();

    _parsePos = 0;
    _servers.clear();

    // Basic structure: Expect server blocks directly (or inside an http block)
    // Assuming no http block for simplicity first.
    try {
        skipWhitespaceAndComments();
        while (_parsePos < _configContent.length()) {
            std::string token = getToken();
            if (token == "server") {
                if (!parseServerBlock()) {
                    return false; // Error during server block parsing
                }
            } else if (!token.empty()){
                 std::cerr << "Error: Unexpected token '" << token << "' outside server block at pos " << _parsePos << std::endl;
                 return false;
            }
            skipWhitespaceAndComments();
        }
    } catch (const std::exception& e) {
        std::cerr << "Configuration parsing error: " << e.what() << std::endl;
        return false;
    }

    if (_servers.empty()) {
         std::cerr << "Error: No server blocks found in configuration file." << std::endl;
         return false;
    }

    // TODO: Add validation (e.g., check for duplicate listens, ensure default server exists per port)

    return true;
}

// --- Parsing Helpers (Basic Implementations) ---

void Config::skipWhitespaceAndComments() {
    while (_parsePos < _configContent.length()) {
        if (std::isspace(_configContent[_parsePos])) {
            _parsePos++;
        } else if (_configContent[_parsePos] == '#') {
            // Skip comment line
            while (_parsePos < _configContent.length() && _configContent[_parsePos] != '\n') {
                _parsePos++;
            }
            if (_parsePos < _configContent.length()) _parsePos++; // Skip the newline itself
        } else {
            break; // Found non-whitespace, non-comment character
        }
    }
}

// Gets the next token (word, symbol like {, }, ;)
std::string Config::getToken() {
    skipWhitespaceAndComments();
    if (_parsePos >= _configContent.length()) {
        return ""; // End of content
    }

    char currentChar = _configContent[_parsePos];

    // Handle symbols
    if (currentChar == '{' || currentChar == '}' || currentChar == ';') {
        _parsePos++;
        return std::string(1, currentChar);
    }

    // Handle quoted strings (optional, simple version doesn't handle escapes)
    /*
    if (currentChar == '\"' || currentChar == '\'') {
        char quote = currentChar;
        _parsePos++;
        size_t start = _parsePos;
        while (_parsePos < _configContent.length() && _configContent[_parsePos] != quote) {
            _parsePos++;
        }
        if (_parsePos >= _configContent.length()) { // Unterminated quote
             throw std::runtime_error("Unterminated quote in configuration file");
        }
        std::string token = _configContent.substr(start, _parsePos - start);
        _parsePos++; // Skip closing quote
        return token;
    }
    */

    // Handle regular words/tokens (until whitespace or symbol)
    size_t start = _parsePos;
    while (_parsePos < _configContent.length() && !std::isspace(_configContent[_parsePos]) &&
           _configContent[_parsePos] != '{' && _configContent[_parsePos] != '}' &&
           _configContent[_parsePos] != ';' && _configContent[_parsePos] != '#') {
        _parsePos++;
    }

    if (_parsePos == start) { // Should not happen if not EOF
         throw std::runtime_error("Parsing error: Stuck at pos " + std::to_string(_parsePos));
    }

    return _configContent.substr(start, _parsePos - start);
}

bool Config::expectToken(const std::string& expected) {
    std::string token = getToken();
    if (token != expected) {
        throw std::runtime_error("Expected token '" + expected + "' but got '" + token + "'");
    }
    return true;
}

// --- Block Parsers (Placeholders/Basic Structure) ---

bool Config::parseServerBlock() {
    if (!expectToken("{")) return false;

    _servers.push_back(ServerConfig()); // Add a new server config
    ServerConfig& currentServer = _servers.back();

    skipWhitespaceAndComments();
    while (true) {
        std::string directive = getToken();
        if (directive == "}") {
            break; // End of server block
        } else if (directive.empty()) {
             throw std::runtime_error("Unexpected EOF inside server block");
        }

        if (directive == "listen") {
            if (!parseListen(currentServer)) return false;
        } else if (directive == "server_name") {
            if (!parseServerName(currentServer)) return false;
        } else if (directive == "error_page") {
            if (!parseErrorPage(currentServer)) return false;
        } else if (directive == "client_max_body_size") {
            if (!parseClientMaxBodySize(currentServer)) return false;
        } else if (directive == "location") {
            if (!parseLocationBlock(currentServer)) return false;
        } else {
            throw std::runtime_error("Unknown directive '" + directive + "' in server block");
        }
    }
    // TODO: Validate server block (e.g., must have listen)
    return true;
}

bool Config::parseLocationBlock(ServerConfig& currentServer) {
    std::string path = getToken();
    if (path.empty() || path == "{" || path == "}" || path == ";") {
        throw std::runtime_error("Expected location path after 'location' directive");
    }
    // TODO: Handle location matching types (prefix, exact, regex - though spec says no regex)

    if (!expectToken("{")) return false;

    currentServer.locations.push_back(Location());
    Location& currentLoc = currentServer.locations.back();
    currentLoc.path = path;
    // Set default allowed methods if none specified later
    currentLoc.allowedMethods.insert("GET");

    skipWhitespaceAndComments();
    while (true) {
        std::string directive = getToken();
        if (directive == "}") {
            break; // End of location block
        } else if (directive.empty()) {
            throw std::runtime_error("Unexpected EOF inside location block");
        }

        if (directive == "root") {
            if (!parseRoot(currentLoc)) return false;
        } else if (directive == "index") {
            if (!parseIndex(currentLoc)) return false;
        } else if (directive == "autoindex") {
            if (!parseAutoindex(currentLoc)) return false;
        } else if (directive == "return") {
            // TODO: Handle return directive
            std::cerr << "Warning: 'return' directive parsing not fully implemented." << std::endl;
            // Skip until semicolon for now
            while (getToken() != ";") { if (_parsePos >= _configContent.length()) throw std::runtime_error("Unexpected EOF after 'return'"); }
        } else if (directive == "cgi_pass") {
            // TODO: Handle cgi_pass directive
            std::cerr << "Warning: 'cgi_pass' directive parsing not fully implemented." << std::endl;
            while (getToken() != ";") { if (_parsePos >= _configContent.length()) throw std::runtime_error("Unexpected EOF after 'cgi_pass'"); }
        } else if (directive == "upload_store") {
            if (!parseUploadStore(currentLoc)) return false;
        } else if (directive == "allowed_methods") {
            parseAllowedMethods(currentLoc);
        } else {
             throw std::runtime_error("Unknown directive '" + directive + "' in location block");
        }
    }
     // TODO: Validate location block
    return true;
}

// --- Directive Parsers (Stubs/Basic Logic) ---
// These need more robust error handling and value parsing

bool Config::parseListen(ServerConfig& currentServer) {
    std::string value = getToken();
    // TODO: Parse host:port format (e.g., 127.0.0.1:8080, *:80, 80)
    // Simple version: Assume only port for now
    try {
        char* endptr;
        long port_l = std::strtol(value.c_str(), &endptr, 10);
        if (*endptr != '\0' || port_l <= 0 || port_l > 65535) {
            throw std::invalid_argument("Invalid port number");
        }
        currentServer.port = static_cast<int>(port_l);
        currentServer.host = "0.0.0.0"; // Default host if only port given
    } catch (...) {
         throw std::runtime_error("Invalid listen directive value: " + value);
    }
    if (!expectToken(";")) return false;
    return true;
}

bool Config::parseServerName(ServerConfig& currentServer) {
    std::string name;
    while ((name = getToken()) != ";") {
        if (name.empty() || name == "{" || name == "}") {
             throw std::runtime_error("Invalid server_name value or missing semicolon");
        }
        currentServer.serverNames.insert(name);
    }
    return true;
}

bool Config::parseErrorPage(ServerConfig& currentServer) {
     std::vector<int> codes;
     std::string token;
     while ((token = getToken()) != ";") {
          if (token.empty() || token == "{" || token == "}") throw std::runtime_error("Invalid error_page format");
          // Check if it's the path (starts with / or .) or a number
          if (token[0] == '/' || token[0] == '.') {
              if (codes.empty()) throw std::runtime_error("Error page path specified before error codes");
              for(std::vector<int>::iterator it = codes.begin(); it != codes.end(); ++it) {
                   currentServer.errorPages[*it] = token;
              }
              if (!expectToken(";")) return false; // Path must be last before semicolon
              return true;
          } else { // Should be an error code
               try {
                    char* endptr;
                    long code_l = std::strtol(token.c_str(), &endptr, 10);
                    if (*endptr != '\0' || code_l < 300 || code_l > 599) throw std::invalid_argument("Invalid code");
                    codes.push_back(static_cast<int>(code_l));
               } catch (...) { throw std::runtime_error("Invalid error code value: " + token); }
          }
     }
     if (codes.empty()) throw std::runtime_error("No error codes specified for error_page");
     // Semicolon was consumed, but no path was found
     throw std::runtime_error("Missing error page path for error_page directive");
}

bool Config::parseClientMaxBodySize(ServerConfig& currentServer) {
    std::string value = getToken();
    size_t multiplier = 1;
    char suffix = '\0';
    if (!value.empty() && (value.back() == 'k' || value.back() == 'K' || value.back() == 'm' || value.back() == 'M')) {
        suffix = std::tolower(value.back());
        value.pop_back(); // Remove suffix for parsing number
        if (suffix == 'k') multiplier = 1024;
        else if (suffix == 'm') multiplier = 1024 * 1024;
    }
     try {
        char* endptr;
        long size_l = std::strtol(value.c_str(), &endptr, 10);
        if (*endptr != '\0' || size_l < 0) throw std::invalid_argument("Invalid size");
        // Check for overflow before multiplying
        if (size_l > static_cast<long>(std::numeric_limits<size_t>::max() / multiplier)) {
             throw std::overflow_error("client_max_body_size value too large");
        }
        currentServer.clientMaxBodySize = static_cast<size_t>(size_l) * multiplier;
    } catch (const std::overflow_error& oe) {
        throw std::runtime_error(oe.what());
    } catch (...) { throw std::runtime_error("Invalid client_max_body_size value: " + value); }
    if (!expectToken(";")) return false;
    return true;
}

bool Config::parseRoot(Location& currentLoc) {
    std::string value = getToken();
    if (value.empty() || value == ";" || value == "{" || value == "}") throw std::runtime_error("Missing root path");
    currentLoc.root = value;
     // TODO: Validate path? (e.g., absolute?)
    if (!expectToken(";")) return false;
    return true;
}

bool Config::parseIndex(Location& currentLoc) {
    std::string file;
    currentLoc.indexFiles.clear(); // Clear defaults if specified
    while ((file = getToken()) != ";") {
        if (file.empty() || file == "{" || file == "}") throw std::runtime_error("Invalid index file or missing semicolon");
        currentLoc.indexFiles.push_back(file);
    }
    if (currentLoc.indexFiles.empty()) throw std::runtime_error("No index files specified after 'index' directive");
    return true;
}

bool Config::parseAutoindex(Location& currentLoc) {
    std::string value = getToken();
    if (value == "on") currentLoc.autoindex = true;
    else if (value == "off") currentLoc.autoindex = false;
    else throw std::runtime_error("Invalid autoindex value: " + value + " (must be 'on' or 'off')");
    if (!expectToken(";")) return false;
    return true;
}

bool Config::parseUploadStore(Location& currentLoc) {
    std::string value = getToken();
    if (value.empty() || value == ";" || value == "{" || value == "}") {
        throw std::runtime_error("Missing upload store path after 'upload_store' directive");
    }
    currentLoc.uploadStore = value;
    // TODO: Validate path? Check permissions?
    if (!expectToken(";")) return false;
    return true;
}

void Config::parseAllowedMethods(Location& location) {
    std::string methodToken;
    location.allowedMethods.clear(); // Clear any defaults if specified

    while (true) {
        methodToken = getToken();
        if (methodToken == ";") {
            break; // End of directive
        }
        if (methodToken.empty()) {
            throw std::runtime_error("Unexpected EOF while parsing allowed_methods");
        }

        // Basic validation (can be expanded)
        if (methodToken == "GET" || methodToken == "POST" || methodToken == "DELETE" || methodToken == "HEAD" || methodToken == "PUT") {
             // std::set automatically handles duplicates, so just insert.
             location.allowedMethods.insert(methodToken);
        } else {
            throw std::runtime_error("Invalid or unsupported HTTP method '" + methodToken + "' in allowed_methods");
        }
    }

    if (location.allowedMethods.empty()) {
        // If no methods were listed before ';', it's arguably an error, or implies default (e.g., GET only?)
        // For now, let's treat it as implicitly allowing GET if nothing else is specified.
         std::cerr << "Warning: 'allowed_methods' specified but no methods listed. Defaulting to GET." << std::endl;
         location.allowedMethods.insert("GET"); // Use insert for set
         // Alternatively, throw an error:
         // throw std::runtime_error("No methods specified for allowed_methods directive");
    }
}

// --- Server/Location Finding Logic (Needs Implementation) ---

const ServerConfig* Config::findServerConfig(const std::string& host, int port, const std::string& serverName) const {
    const ServerConfig* defaultServer = NULL;
    for (std::vector<ServerConfig>::const_iterator it = _servers.begin(); it != _servers.end(); ++it) {
        const ServerConfig& server = *it;
        if (server.port == port && (server.host == "0.0.0.0" || server.host == host)) {
            // Match port and host (or wildcard host)
            if (!defaultServer) {
                 defaultServer = &server; // First match for this host:port is default
            }
            if (server.serverNames.count(serverName)) {
                return &server; // Exact server_name match
            }
            // TODO: Add wildcard server_name matching if needed
        }
    }
    return defaultServer; // Return default if no specific name matched
}

const Location* ServerConfig::findLocation(const std::string& requestPath) const {
    const Location* bestMatch = NULL;
    size_t longestMatchLen = 0;

    for (std::vector<Location>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        const Location& loc = *it;
        // Simple prefix matching
        if (Utils::startsWith(requestPath, loc.path)) {
            // Check if this match is longer than the current best match
            if (loc.path.length() > longestMatchLen) {
                longestMatchLen = loc.path.length();
                bestMatch = &loc;
            }
            // Exact match is usually preferred, treat '/' special?
            // Nginx has more complex matching rules (prefix, exact, regex)
            // For now, longest prefix match wins.
        }
    }
    return bestMatch;
} 