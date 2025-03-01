#include "../includes/Server.hpp"
#include "../includes/Request.hpp"
#include "../includes/Response.hpp"

int main()
{
    // Server server;

    // server.run();
    // return 0;
    // std::string test_request = 
    //     "GET /test HTTP/1.1\r\n"
    //     "Host: localhost:8080\r\n"
    //     "User-Agent: curl/7.68.0\r\n"
    //     "\r\n";

    // Request request(test_request);
    
    // std::cout << "Method: " << request.getMethod() << std::endl;
    // std::cout << "Path: " << request.getPath() << std::endl;
    // std::cout << "Version: " << request.getVersion() << std::endl;
    // std::cout << "Host header: " << request.getHeader("Host") << std::endl;
    Response response(200);
    response.setHeader("Content-Type", "text/plain");
    response.setBody("Hello, World!");  
    std::cout << "Response:\n" << response.toString() << std::endl;
    Server server;
    server.run();
    return 0;
}