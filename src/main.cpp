#include <iostream>
#include <string>
#include <vector> // Include for future use if needed
#include "Server.hpp"
#include "Config.hpp" // Include Config header

int main(int argc, char* argv[]) {
    // Determine configuration file path
    std::string config_file;
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [configuration_file]" << std::endl;
        return 1;
    } else if (argc == 2) {
        config_file = argv[1];
    } else {
        config_file = "configs/default.conf"; // Default configuration path
        std::cout << "No configuration file provided. Using default: " << config_file << std::endl;
    }

    try {
        // Load configuration
        Config config(config_file);
        if (!config.load()) { // Assume Config::load() returns bool for success
             std::cerr << "Error: Failed to load configuration file: " << config_file << std::endl;
             return 1;
        }

        // Create and run the server
        Server server(config);
        server.run();

    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "An unknown error occurred." << std::endl;
        return 1;
    }


    std::cout << "Server shutting down." << std::endl;
    return 0;
} 