#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <map>

namespace Utils {
    // String manipulation
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& s, char delimiter);
    bool endsWith(const std::string& value, const std::string& ending);
    bool startsWith(const std::string& value, const std::string& starting);
    std::string& toLower(std::string& s);

    // File system
    bool fileExists(const std::string& path);
    bool isDirectory(const std::string& path);
    long getFileSize(const std::string& path);
    std::string getFileExtension(const std::string& path);

    // HTTP related
    std::string getMimeType(const std::string& filePath);
    std::string getHttpStatusMessage(int statusCode);
    std::string getCurrentHttpDate();

    // Network
    // Add network related utils if needed, e.g., parsing host/port

}

#endif // UTILS_HPP 