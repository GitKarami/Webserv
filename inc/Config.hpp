#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <map>

class Config {
public:
    Config(const std::string& filename);
    ~Config();

    // Load and parse the configuration file
    bool load();

    // Methods to access configuration values (placeholders)
    // e.g., std::vector<int> getPorts() const;
    // std::vector<std::string> getServerNames(int port) const;
    // std::string getErrorPage(int statusCode) const;
    // size_t getClientMaxBodySize() const;
    // ... other getter methods based on subject requirements ...

private:
    std::string _filename;
    // Data structures to store parsed configuration
    // e.g., std::map<int, ServerConfig> _serverConfigs;

    // Private helper methods for parsing
    bool parseFile(); // Renamed from parseLine for clarity
    // ... other parsing helpers ...
};

#endif // CONFIG_HPP 