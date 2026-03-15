#include "tank_server.hpp"

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    unsigned short port = 7777;

    if (argc >= 2) {
        port = static_cast<unsigned short>(std::atoi(argv[1]));
    }

    try {
        std::cout << "Tank server starting on port " << port << "...\n";
        lve::net::TankServer server{ port };
        server.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}