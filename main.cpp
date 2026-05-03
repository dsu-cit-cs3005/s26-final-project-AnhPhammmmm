#include "Arena.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file_path>\n";
        return 1;
    }

    std::string configPath = argv[1];
    
    std::cout << "--- Initializing Robot Arena ---\n";
    Arena gameArena(configPath);

    std::cout << "--- Loading and Compiling Robots ---\n";
    gameArena.loadRobots();

    std::cout << "--- Starting Simulation ---\n";
    gameArena.run();

    std::cout << "\nSimulation Complete. Check the logs above for the winner!\n";

    return 0;
}