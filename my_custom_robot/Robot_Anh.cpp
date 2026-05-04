#include "RobotBase.h"
#include <vector>
#include <cstdlib>   
#include <ctime>     
#include <cmath>   
#include <algorithm> 

class Robot_Anh : public RobotBase {
private:
    std::vector<RadarObj> objFound;
    int previous_health;

public:
    Robot_Anh() : RobotBase(2, 5, hammer) {
        m_name = "Anh_Bot";  
        m_character = 'A';
        previous_health = get_health();
        
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
    }

    virtual void get_radar_direction(int& radar_direction) override {
        radar_direction = 0; 
    }

    virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override {
        objFound = radar_results;
    }

    virtual bool get_shot_location(int& shot_row, int& shot_col) override {
        for (const auto& obj : objFound) {
            if (obj.m_type == 'R') {
                shot_row = obj.m_row;
                shot_col = obj.m_col;
                return true;
            }
        }
        return false;
    }

    virtual void get_move_direction(int& move_direction, int& move_distance) override {
        std::vector<bool> attemp(8, false);
        int current_row, current_col;
        get_current_location(current_row, current_col);

        while (true) {
            if (std::all_of(attemp.begin(), attemp.end(), [](bool v) { return v; })) {
                break;
            }

            bool canGo = true;
            int dir = (std::rand() % 8) + 1; 

            if (attemp[dir - 1]) continue;
            attemp[dir - 1] = true;

            int cR = current_row + directions[dir].first;
            int cC = current_col + directions[dir].second;

            if (cR < 0 || cR >= m_board_row_max || cC < 0 || cC >= m_board_col_max) {
                canGo = false;
            }

            if (canGo) {
                for (const auto& obj : objFound) {
                    if (cR == obj.m_row && cC == obj.m_col) {
                        canGo = false;
                        break;
                    }
                } 
            }

            if (canGo) {
                move_direction = dir;
                if (previous_health > get_health()) {
                    move_distance = 2;   
                } else {
                    move_distance = 1;
                }
                previous_health = get_health(); 
                return;
            }
        }
        move_direction = 0;
        move_distance = 0;
        previous_health = get_health();
    }
};

extern "C" RobotBase* create_robot() 
{
    return new Robot_Anh();
}

extern "C" const char* robot_summary()
{
    return "Crushes nearby enemies or flees if taking damage.";
}