#pragma once
#include <vector>
#include <string>
#include "RobotBase.h"
#include "RadarObj.h"

struct Cell {
    char type; 
    RobotBase* occupant;
    bool is_occupied;
};

class Arena {
private:
    int width, height, maxRounds;
    std::vector<std::vector<Cell>> grid;
    std::vector<RobotBase*> robots;
    double sleepInterval;
    std::vector<void*> library_handles;
    bool gameStateLive;

public:
    Arena(const std::string& configFile);
    ~Arena();
    void loadConfig(const std::string& filename);
    void loadRobots(); 
    void run();
    void render();
    void performRadar(RobotBase* robot, int dir, std::vector<RadarObj>& results);
    char getVisibleType(int r, int c);
    void handleCombat(RobotBase* rb, int r, int c);
    void handleMovement(RobotBase* rb, int dir, int dist);
    void applyDamage(RobotBase* target, int base);
    bool checkWinCondition();
};