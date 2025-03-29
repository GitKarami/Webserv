#include "Utils.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <ctime>
#include <iomanip>
#include <map>

namespace Utils {

    // --- String Manipulation ---
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r\f\v");
        if (std::string::npos == first) {
            return str;
        }
        size_t last = str.find_last_not_of(" \t\n\r\f\v");
        return str.substr(first, (last - first + 1));
    }

    std::vector<std::string> split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    bool endsWith(const std::string& value, const std::string& ending) {
        if (ending.size() > value.size()) return false;
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

     bool startsWith(const std::string& value, const std::string& starting) {
        if (starting.size() > value.size()) return false;
        return std::equal(starting.begin(), starting.end(), value.begin());
    }

    std::string& toLower(std::string& s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return s;
    }

    // --- File System ---
    bool fileExists(const std::string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }

    bool isDirectory(const std::string& path) {
        struct stat buffer;
        if (stat(path.c_str(), &buffer) != 0) {
            return false; // Path doesn't exist
        }
        return S_ISDIR(buffer.st_mode);
    }

     long getFileSize(const std::string& path) {
        struct stat buffer;
        if (stat(path.c_str(), &buffer) != 0) {
            return -1; // Error or file doesn't exist
        }
        return buffer.st_size;
    }

    std::string getFileExtension(const std::string& path) {
        size_t dotPos = path.rfind('.');
        if (dotPos == std::string::npos || dotPos == 0) { // No extension or starts with dot
            return "";
        }
        return path.substr(dotPos); // Includes the dot
    }

    // --- HTTP Related ---
    std::string getMimeType(const std::string& filePath) {
        static std::map<std::string, std::string> mimeTypes;
        if (mimeTypes.empty()) {
            // Initialize known MIME types
            mimeTypes[".html"] = "text/html";
            mimeTypes[".htm"] = "text/html";
            mimeTypes[".css"] = "text/css";
            mimeTypes[".js"] = "application/javascript";
            mimeTypes[".json"] = "application/json";
            mimeTypes[".xml"] = "application/xml";
            mimeTypes[".jpg"] = "image/jpeg";
            mimeTypes[".jpeg"] = "image/jpeg";
            mimeTypes[".png"] = "image/png";
            mimeTypes[".gif"] = "image/gif";
            mimeTypes[".ico"] = "image/x-icon";
            mimeTypes[".svg"] = "image/svg+xml";
            mimeTypes[".txt"] = "text/plain";
            mimeTypes[".pdf"] = "application/pdf";
            mimeTypes[".zip"] = "application/zip";
            // Add more as needed
        }

        std::string ext = getFileExtension(filePath);
        toLower(ext);
        std::map<std::string, std::string>::const_iterator it = mimeTypes.find(ext);
        if (it != mimeTypes.end()) {
            return it->second;
        }
        return "application/octet-stream"; // Default binary type
    }

    std::string getHttpStatusMessage(int statusCode) {
        static std::map<int, std::string> statusMessages;
        if (statusMessages.empty()) {
            // Common HTTP status codes
            statusMessages[200] = "OK";
            statusMessages[201] = "Created";
            statusMessages[204] = "No Content";
            statusMessages[301] = "Moved Permanently";
            statusMessages[302] = "Found";
            statusMessages[304] = "Not Modified";
            statusMessages[400] = "Bad Request";
            statusMessages[401] = "Unauthorized";
            statusMessages[403] = "Forbidden";
            statusMessages[404] = "Not Found";
            statusMessages[405] = "Method Not Allowed";
            statusMessages[413] = "Payload Too Large";
            statusMessages[500] = "Internal Server Error";
            statusMessages[501] = "Not Implemented";
            statusMessages[503] = "Service Unavailable";
            statusMessages[505] = "HTTP Version Not Supported";
        }

        std::map<int, std::string>::const_iterator it = statusMessages.find(statusCode);
        if (it != statusMessages.end()) {
            return it->second;
        }
        return "Unknown Status"; // Default for unlisted codes
    }

    std::string getCurrentHttpDate() {
        std::time_t now = std::time(0);
        std::tm* gmtm = std::gmtime(&now);
        if (!gmtm) {
            return ""; // Error getting time
        }

        char buf[100];
        // Format: RFC 1123 (HTTP Date format)
        if (std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmtm)) {
            return std::string(buf);
        }
        return ""; // Error formatting time
    }

} 