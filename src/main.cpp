#include "Config.hpp"
#include "Server.hpp"
#include <iostream>
#include <string>
#include <csignal> // Include for signal handling setup in Server

int main(int argc, char* argv[]) {
    // Determine configuration file path
    std::string configFilename = "./config/default.conf"; // Default config file path
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [config_file]" << std::endl;
        return 1;
    } else if (argc == 2) {
        configFilename = argv[1];
    }

    std::cout << "Using configuration file: " << configFilename << std::endl;

    // Load Configuration
    Config serverConfig;
    try {
        if (!serverConfig.loadFromFile(configFilename)) {
            std::cerr << "Error: Failed to load configuration from " << configFilename << std::endl;
            return 1;
        }
        std::cout << "Configuration loaded successfully." << std::endl;
        // Optional: Print loaded configuration summary here if needed

    } catch (const std::exception& e) {
        std::cerr << "Error during configuration parsing: " << e.what() << std::endl;
        return 1;
    }

    // Create and Initialize Server
    Server webserver(serverConfig);
    if (!webserver.init()) {
        std::cerr << "Error: Failed to initialize server." << std::endl;
        return 1;
    }

    // Run the Server
    std::cout << "Starting server... Press Ctrl+C to exit." << std::endl;
    webserver.run(); // This call blocks until the server is stopped

    std::cout << "Server stopped gracefully." << std::endl;

    return 0;
} 