#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include "Location.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>

// Represents a server block in the config
struct ServerConfig {
    std::string host;
    int port;
    std::set<std::string> serverNames;
    std::map<int, std::string> errorPages; // Map error code to file path
    size_t clientMaxBodySize; // In bytes
    std::vector<Location> locations;
    // Add pointer back to main Config or other shared settings if needed

    ServerConfig() : port(80), clientMaxBodySize(1024 * 1024) {} // Default port 80, default max body 1MB

    // Function to find the best matching location for a given request path
    const Location* findLocation(const std::string& requestPath) const;
};

#endif // SERVER_CONFIG_HPP 