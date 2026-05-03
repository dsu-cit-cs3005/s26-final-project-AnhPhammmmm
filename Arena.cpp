#include "Arena.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <dlfcn.h>
#include <unistd.h>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <ctime>

namespace fs = std::filesystem;

Arena::Arena(const std::string& configFile) : width(20), height(20), maxRounds(10000), sleepInterval(0.5) {
    std::srand(std::time(0));
    loadConfig(configFile);
}

Arena::~Arena() {
    for (RobotBase* robot : robots) {
        delete robot;
    }
    for (void* handle : library_handles) {
        dlclose(handle);
    }
}

void Arena::loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    int numF = 0, numP = 0, numM = 0;
    std::string gameState = "false";

    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            for (char &c : line) if (c == ':') c = ' ';
            
            std::stringstream ss(line);
            std::string key;
            ss >> key;

            if (key == "Arena_Size") ss >> height >> width;
            else if (key == "Max_Rounds") ss >> maxRounds;
            else if (key == "Sleep_interval") ss >> sleepInterval;
            else if (key == "Game_State_Live") ss >> gameState;
            else if (key == "Flamethrowers") ss >> numF;
            else if (key == "Pits") ss >> numP;
            else if (key == "Mounds") ss >> numM;
        }
    }
    
    grid.assign(height, std::vector<Cell>(width, {'.', nullptr}));

    gameStateLive = (gameState == "true");
    
    auto place = [&](char type, int count) {
        for (int i = 0; i < count; ++i) {
            int r, c;
            do { r = std::rand() % height; c = std::rand() % width; } 
            while (grid[r][c].type != '.');
            grid[r][c].type = type;
        }
    };

    place('F', numF);
    place('P', numP);
    place('M', numM);
}

void Arena::loadRobots() {
    std::string robot_dir = "."; 

    for (const auto& entry : fs::directory_iterator(robot_dir)) {
        std::string filename = entry.path().filename().string(); 
        
        if (filename.find("Robot_") == 0 && filename.ends_with(".cpp")) {
            std::string shared_lib = "./" + filename.substr(0, filename.find_last_of('.')) + ".so";
            std::string compile_cmd = "g++ -shared -fPIC -o " + shared_lib + " " + filename + " RobotBase.o -I. -std=c++20";
            std::cout << "Compiling " << filename << " to " << shared_lib << "...\n";

            if (std::system(compile_cmd.c_str()) != 0) {
                std::cerr << "Failed to compile " << filename << " with command: " << compile_cmd << std::endl;
                continue;
            }

            void* handle = dlopen(shared_lib.c_str(), RTLD_LAZY);
            if (!handle) {
                std::cerr << "Failed to load " << shared_lib << ": " << dlerror() << std::endl;
                continue;
            }

            RobotFactory create_robot = (RobotFactory)dlsym(handle, "create_robot");
            if (!create_robot) {
                dlclose(handle);
                continue;
            }

            RobotBase* new_robot = create_robot();
            new_robot->set_boundaries(height, width); 

            int r, c;
            do {
                r = std::rand() % height;
                c = std::rand() % width;
            } while (grid[r][c].type != '.' || grid[r][c].occupant != nullptr);

            new_robot->move_to(r, c);
            grid[r][c].occupant = new_robot;
            
            robots.push_back(new_robot);
            library_handles.push_back(handle); 

            std::cout << "Successfully Loaded: " << new_robot->m_name << "\n";
        }
    }
}

void Arena::run() {
    int currentRound = 1;
    while (currentRound <= maxRounds) {
        std::cout << "        " << "\n=========== Round " << currentRound << " ===========\n";
        render();

        if (checkWinCondition()) break;

        for (RobotBase* rb : robots) {
            if (rb->get_health() <= 0) continue;

            int rDir;
            rb->get_radar_direction(rDir);
            std::vector<RadarObj> results;
            performRadar(rb, rDir, results);
            rb->process_radar_results(results);

            int sR, sC;
            if (rb->get_shot_location(sR, sC)) {
                handleCombat(rb, sR, sC);
            } else {
                int mDir, mDist;
                rb->get_move_direction(mDir, mDist);
                handleMovement(rb, mDir, mDist);
            }
        }
        if (gameStateLive) {
            usleep(static_cast<useconds_t>(sleepInterval * 1000000));
        }
        currentRound++;
    }
}

void Arena::performRadar(RobotBase* robot, int dir, std::vector<RadarObj>& results) {
    int rR, rC;
    robot->get_current_location(rR, rC);

    if (dir == 0) { 
        for (int i = 1; i <= 8; ++i) {
            int cR = rR + directions[i].first, cC = rC + directions[i].second;
            if (cR >= 0 && cR < height && cC >= 0 && cC < width) {
                char seen = getVisibleType(cR, cC);
                if (seen != '.') {
                    results.push_back(RadarObj(seen, cR, cC));
                }
            }
        }
        return;
    }

    int left = (dir == 1) ? 8 : dir - 1;
    int right = (dir == 8) ? 1 : dir + 1;
    int dirs[3] = {dir, left, right};

    int main_dr = directions[dir].first;
    int main_dc = directions[dir].second;

    for (int step = 1; step < std::max(width, height); ++step) {
        for (int d : dirs) {
            int start_R = rR + directions[d].first;
            int start_C = rC + directions[d].second;

            int cR = start_R + (main_dr * (step - 1));
            int cC = start_C + (main_dc * (step - 1));

            if (cR >= 0 && cR < height && cC >= 0 && cC < width) {
                char seen = getVisibleType(cR, cC);
                if (seen != '.') {
                    results.push_back(RadarObj(seen, cR, cC));
                }
            }
        }
    }
}

char Arena::getVisibleType(int r, int c) {
    if (grid[r][c].occupant != nullptr) {
        return (grid[r][c].occupant->get_health() > 0) ? 'R' : 'X';
    }
    return grid[r][c].type; 
}

void Arena::handleMovement(RobotBase* robot, int dir, int dist) {
    if (dir <= 0 || dist <= 0) return;
    int speed = std::min(dist, robot->get_move_speed());
    
    for (int i = 0; i < speed; ++i) {
        int r, c;
        robot->get_current_location(r, c);
        int nR = r + directions[dir].first;
        int nC = c + directions[dir].second;

        if (nR < 0 || nR >= height || nC < 0 || nC >= width) break;
        if (grid[nR][nC].type == 'M' || grid[nR][nC].type == 'X' || grid[nR][nC].occupant != nullptr) break;

        grid[r][c].occupant = nullptr;
        robot->move_to(nR, nC);
        grid[nR][nC].occupant = robot;

        if (grid[nR][nC].type == 'F') {
            applyDamage(robot, (std::rand() % 21) + 30);
            if (robot->get_health() <= 0) { grid[nR][nC].type = 'X'; break; }
        } else if (grid[nR][nC].type == 'P') {
            robot->disable_movement();
            break;
        }
    }
}

void Arena::handleCombat(RobotBase* shooter, int tR, int tC) {
    int rR, rC; 
    shooter->get_current_location(rR, rC);
    WeaponType w = shooter->get_weapon();

    switch (w) {
        case railgun: {
            double dR = tR - rR, dC = tC - rC;
            double steps = std::max(std::abs(dR), std::abs(dC));
            if (steps == 0) break;
            double sR = dR/steps, sC = dC/steps;
            double currR = rR + sR, currC = rC + sC;
            while (currR >= 0 && currR < height && currC >= 0 && currC < width) {
                int cR = std::round(currR), cC = std::round(currC);
                if (grid[cR][cC].occupant && grid[cR][cC].occupant != shooter) 
                    applyDamage(grid[cR][cC].occupant, (std::rand() % 11) + 10);
                currR += sR; currC += sC;
            }
            break;
        }
        case grenade: {
            if (shooter->get_grenades() <= 0) break;
            shooter->decrement_grenades();
            for (int r = tR-1; r <= tR+1; ++r)
                for (int c = tC-1; c <= tC+1; ++c)
                    if (r >= 0 && r < height && c >= 0 && c < width && grid[r][c].occupant)
                        applyDamage(grid[r][c].occupant, (std::rand() % 31) + 10);
            break;
        }
        case flamethrower: {
            double dR = tR - rR, dC = tC - rC;
            double dist = std::max(std::abs(dR), std::abs(dC));
            if (dist == 0) break;
            int dir = 0; double bestDot = -2.0;
            for(int i=1; i<=8; ++i) {
                double dot = (directions[i].first * (dR/dist)) + (directions[i].second * (dC/dist));
                if (dot > bestDot) { bestDot = dot; dir = i; }
            }

            int l = (dir == 1) ? 8 : dir - 1, r = (dir == 8) ? 1 : dir + 1;
            int dirs[3] = {dir, l, r};
            int main_dr = directions[dir].first;
            int main_dc = directions[dir].second;
            for (int step = 1; step <= 4; ++step) {
                for (int d : dirs) {
                    int start_R = rR + directions[d].first;
                    int start_C = rC + directions[d].second;

                    int cR = start_R + (main_dr * (step - 1));
                    int cC = start_C + (main_dc * (step - 1));

                    if (cR >= 0 && cR < height && cC >= 0 && cC < width && grid[cR][cC].occupant)
                        applyDamage(grid[cR][cC].occupant, (std::rand() % 21) + 30);
                }
            }
            break;
        }
        case hammer: {
            if (std::abs(tR - rR) <= 1 && std::abs(tC - rC) <= 1) {
                if (grid[tR][tC].occupant) 
                    applyDamage(grid[tR][tC].occupant, (std::rand() % 11) + 50);
            }
            break;
        }
    }
}

void Arena::applyDamage(RobotBase* target, int base) {
    if (!target || target->get_health() <= 0) return;
    double multi = 1.0 - (target->get_armor() * 0.1);
    target->take_damage(static_cast<int>(base * multi));
    target->reduce_armor(1);
    if (target->get_health() <= 0) {
        int r, c; target->get_current_location(r, c);
        grid[r][c].type = 'X';
        grid[r][c].occupant = nullptr;
    }
}

void Arena::render() {
    std::cout << "    ";
    for (int c = 0; c < width; ++c) printf("%2d ", c);
    std::cout << "\n";
    for (int r = 0; r < height; ++r) {
        printf("%2d  ", r);
        for (int c = 0; c < width; ++c) {
            if (grid[r][c].occupant) {
                char icon = (grid[r][c].occupant->get_health() > 0) ? 'R' : 'X';
                std::cout << icon << grid[r][c].occupant->m_character << " ";
            } else std::cout << " " << grid[r][c].type << " ";
        }
        std::cout << "\n";
    }
    for (auto* rb : robots) std::cout << rb->print_stats() << (rb->get_health() <= 0 ? " [DEAD]" : "") << "\n";
}

bool Arena::checkWinCondition() {
    int alive = 0; RobotBase* winner = nullptr;
    for (auto* r : robots) if (r->get_health() > 0) { alive++; winner = r; }
    if (alive <= 1) {
        if (winner) std::cout << "\n*** WINNER: " << winner->m_name << " ***\n";
        else std::cout << "\n*** DRAW: ALL DESTROYED ***\n";
        return true;
    }
    return false;
}