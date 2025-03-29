#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "ServerConfig.hpp"
#include <string>
#include <vector>
#include <map>

// Main class to parse and hold the entire configuration
class Config {
public:
    Config();
    ~Config();

    bool loadFromFile(const std::string& filename);

    // Getters
    const std::vector<ServerConfig>& getServerConfigs() const;

    // Finds the best matching ServerConfig for a request (based on host:port and server_name)
    const ServerConfig* findServerConfig(const std::string& host, int port, const std::string& serverName) const;

private:
    std::vector<ServerConfig> _servers;
    // Add http block settings if needed (global settings)

    // Parsing state and helpers
    std::string _configContent;
    size_t _parsePos;

    bool parseHttpBlock(); // Placeholder
    bool parseServerBlock();
    bool parseLocationBlock(ServerConfig& currentServer);
    std::string getToken(); // Helper to get next token/word/symbol
    bool expectToken(const std::string& expected); // Check for specific token

    // Helper to parse directives like listen, server_name, root, etc.
    bool parseListen(ServerConfig& currentServer);
    bool parseServerName(ServerConfig& currentServer);
    bool parseErrorPage(ServerConfig& currentServer);
    bool parseClientMaxBodySize(ServerConfig& currentServer);
    bool parseRoot(Location& currentLoc);
    bool parseIndex(Location& currentLoc);
    bool parseAutoindex(Location& currentLoc);
    bool parseReturn(Location& location);
    bool parseCgiPass(Location& location);
    bool parseUploadStore(Location& location);

    // Helper function for allowed methods
    void parseAllowedMethods(Location& location);

    // Utility methods
    void skipWhitespaceAndComments();
};

#endif // CONFIG_HPP 