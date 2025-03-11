#include "../includes/Server.hpp"


int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <config file>" << std::endl;
        return 1;
    }
    Server server(argv[1]);
    server.run();
    return 0;
}