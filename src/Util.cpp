#include "Util.hpp"

#include <cmath>

std::string file_size_to_string(size_t bytes) 
{
    const char* units[] = {"B", "K", "M", "G", "T"};
    int unitIndex = 0;
    float bytesf = bytes;
    while (bytesf >= 1024 && unitIndex < 4) 
    {
        bytesf /= 1024;
        unitIndex++;
    }
    
    if(unitIndex == 0)
        return std::to_string(bytes) + " " + units[unitIndex];

    bytesf = std::round(bytesf * 10) / 10;
    std::string sizeStr = std::to_string(bytesf);\
    sizeStr.erase(sizeStr.find_last_of('.') + 2);
    return sizeStr + " " + units[unitIndex];
}