#ifndef LOCATION_HPP
#define LOCATION_HPP

#include <string>
#include <vector>
#include <set>
#include <map>

// Represents a location block in the config
struct Location {
    std::string path;
    std::string root;
    std::vector<std::string> indexFiles;
    std::set<std::string> allowedMethods; // Allowed HTTP methods (GET, POST, DELETE)
    bool autoindex;
    std::pair<int, std::string> redirect; // Redirect code and URL (0 if no redirect)
    std::string cgiPath; // Path to CGI executable
    std::string uploadStore; // Directory to store uploads
    // Add other location-specific settings if needed

    Location() : autoindex(false), redirect({0, ""}) {}
};

#endif // LOCATION_HPP 