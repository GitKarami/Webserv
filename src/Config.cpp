#include "Config.hpp"
#include <fstream>
#include <iostream> // Example include
#include <sstream> // For parsing
#include <vector>
#include <map> // For storing parsed data
#include <algorithm> // for std::find
#include <stack> // Include stack for brace matching

// --- TODO: Define data structures to hold parsed config ---
// Example structure (needs refinement based on full subject)
struct LocationConfig {
    std::string path;
    std::string root;
    std::vector<std::string> allowedMethods;
    std::vector<std::string> indexFiles;
    bool autoindex;
    // ... other location directives (cgi, upload_path, return, limit_except etc.)

    LocationConfig() : autoindex(false) {} // Default autoindex off
};

struct ServerConfig {
    std::vector<std::pair<std::string, int> > listens; // host:port pairs
    std::vector<std::string> serverNames;
    std::string root; // Default root for the server
    std::vector<std::string> indexFiles; // Default index files
    std::map<int, std::string> errorPages; // map status code to path
    size_t clientMaxBodySize;
    std::vector<LocationConfig> locations;

    ServerConfig() : clientMaxBodySize(1048576) {} // Default 1MB
};
// --- End Config Structures ---

Config::Config(const std::string& filename) : _filename(filename) {
    // Constructor implementation
    // Consider calling load() here or requiring explicit call
    std::cout << "Config object created for file: " << _filename << std::endl;
    // load();
}

Config::~Config() {
    // Destructor implementation
}

// Load and parse the configuration file
bool Config::load() {
    std::ifstream configFileStream(_filename.c_str());
    if (!configFileStream.is_open()) {
        std::cerr << "Error: Could not open configuration file: " << _filename << std::endl;
        return false;
    }
    std::cout << "Loading configuration from: " << _filename << std::endl;

    std::string line;
    int lineNumber = 0;
    ServerConfig currentServer;
    bool in_server_block = false;
    std::stack<char> brace_stack; // Use a stack to track nested braces

    while (std::getline(configFileStream, line)) {
        lineNumber++;

        // Basic cleanup and comment skipping
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        first = line.find_first_not_of(" \t");
         if (first == std::string::npos || line.empty()) continue; // Skip empty lines after removing comments
        size_t last = line.find_last_not_of(" \t");
        line = line.substr(first, (last - first + 1));
         if (line.empty()) continue;


        // Basic block handling using stack
        if (line.find('{') != std::string::npos) {
            // Simple check for server block start
            if (line.find("server") == 0 && line.find('{') > line.find("server")) {
                if (in_server_block) {
                     std::cerr << "Error: Nested server blocks not supported (line " << lineNumber << ")" << std::endl;
                     return false;
                }
                 in_server_block = true;
                 currentServer = ServerConfig(); // Reset for new server
                 brace_stack.push('{');
                 std::cout << "Entering server block (line " << lineNumber << ")" << std::endl;
                 continue; // Process next line
            }
            // Simple check for location block start *within* a server block
            else if (in_server_block && line.find("location") == 0 && line.find('{') > line.find("location")) {
                 brace_stack.push('{'); // Push location block brace
                 std::cout << "Entering location block (line " << lineNumber << ") - skipping content." << std::endl;
                 continue; // Process next line
            }
            // Handle other opening braces if necessary, or flag as error if unexpected
            else {
                 // For simplicity, assume any other '{' is part of a directive value for now
                 // or potentially part of a location block line we skipped parsing arguments from
                 if (!brace_stack.empty()) { // If we are inside location block, just assume it's content
                     brace_stack.push('{');
                 } else {
                    std::cerr << "Error: Unexpected '{' (line " << lineNumber << ")" << std::endl;
                    return false;
                 }
            }
        }

        if (line.find('}') != std::string::npos) {
             if (brace_stack.empty()) {
                 std::cerr << "Error: Unexpected '}' (line " << lineNumber << ")" << std::endl;
                 return false;
             }
             brace_stack.pop(); // Pop the matching brace

             if (brace_stack.empty() && in_server_block) { // If stack is now empty, it's the end of the server block
                 in_server_block = false;
                 // TODO: Store the completed currentServer
                 // _servers.push_back(currentServer);
                 std::cout << "Exiting server block (line " << lineNumber << ")" << std::endl;
                 continue;
             } else if (!brace_stack.empty()) {
                 // Still inside nested blocks (e.g., location)
                 std::cout << "Exiting nested block (line " << lineNumber << ")" << std::endl;
                 continue;
             } else {
                 // Should not happen if logic is correct
                 std::cerr << "Error: Logic error handling '}' (line " << lineNumber << ")" << std::endl;
                 return false;
             }
        }

        // --- Directive Parsing ---
        // Only parse if inside server block but *outside* nested blocks (brace_stack.size() == 1)
        if (in_server_block && brace_stack.size() == 1) {
            std::istringstream lineStream(line);
            std::string directive;
            lineStream >> directive;

            // Remove trailing semicolon from the directive itself if present
             if (!directive.empty() && directive.back() == ';') directive.pop_back();

            // Parse known server-level directives
            if (directive == "listen") {
                 std::string listen_arg;
                 lineStream >> listen_arg;
                 if (!listen_arg.empty() && listen_arg.back() == ';') listen_arg.pop_back();
                 // TODO: Parse host:port properly
                 if (listen_arg == "127.0.0.1:8080") {
                     currentServer.listens.push_back(std::make_pair("127.0.0.1", 8080));
                 } else { std::cerr << "Warning: Unrecognized listen format (line " << lineNumber << "): " << line << std::endl; }
            } else if (directive == "server_name") {
                 std::string name;
                 while(lineStream >> name) {
                     if (!name.empty() && name.back() == ';') name.pop_back();
                     if (name.empty()) break;
                     currentServer.serverNames.push_back(name);
                 }
            } else if (directive == "root") {
                lineStream >> currentServer.root;
                if (!currentServer.root.empty() && currentServer.root.back() == ';') currentServer.root.pop_back();
            } else if (directive == "index") {
                 std::string index_file;
                 while(lineStream >> index_file) {
                     if (!index_file.empty() && index_file.back() == ';') index_file.pop_back();
                      if (index_file.empty()) break;
                     currentServer.indexFiles.push_back(index_file);
                 }
            } else if (directive == "error_page") {
                int code;
                std::string page_path;
                if (lineStream >> code >> page_path) {
                     if (!page_path.empty() && page_path.back() == ';') page_path.pop_back();
                    currentServer.errorPages[code] = page_path;
                } else { std::cerr << "Warning: Failed to parse error_page (line " << lineNumber << "): " << line << std::endl; }
             }
             // Ignore location directives at this level
             else if (directive == "location") {
                  // Already handled block opening above, ignore the line itself here
             }
             else {
                std::cerr << "Warning: Unknown server directive '" << directive << "' (line " << lineNumber << ")" << std::endl;
            }
        } else if (!in_server_block && !line.empty()) {
            // Outside any block - should be an error unless it's a top-level directive (e.g., 'worker_processes' in Nginx)
            std::cerr << "Warning: Directive outside server block ignored (line " << lineNumber << "): " << line << std::endl;
        } else if (in_server_block && brace_stack.size() > 1) {
             // Inside a nested block (location) - content is skipped currently
             // std::cout << "Skipping content inside nested block (line " << lineNumber << "): " << line << std::endl;
        }

    } // end while getline

    if (!brace_stack.empty()) {
        std::cerr << "Error: Unterminated block at end of file. Brace stack size: " << brace_stack.size() << std::endl;
        return false;
    }
     if (in_server_block) { // Should have been set to false by closing brace if file is valid
         std::cerr << "Error: Server block not properly closed at end of file." << std::endl;
         return false;
     }


    // --- Placeholder: Print parsed data ---
    std::cout << "--- Parsed Config (Placeholder) ---" << std::endl;
    for(size_t i = 0; i < currentServer.listens.size(); ++i) std::cout << "Listen: " << currentServer.listens[i].first << ":" << currentServer.listens[i].second << std::endl;
    std::cout << "Root: " << currentServer.root << std::endl;
    std::cout << "Index: "; for(size_t i = 0; i< currentServer.indexFiles.size(); ++i) std::cout << currentServer.indexFiles[i] << " "; std::cout << std::endl;
    for(std::map<int, std::string>::const_iterator it = currentServer.errorPages.begin(); it != currentServer.errorPages.end(); ++it) std::cout << "Error Page " << it->first << ": " << it->second << std::endl;
    std::cout << "---------------------------------" << std::endl;

    return true; // Assume success if no fatal parse errors occurred
}

// Placeholder for parsing logic
bool Config::parseFile() {
    // Implementation needed
    return true;
}

// Implement Config methods here
// e.g., bool Config::load() { ... } 