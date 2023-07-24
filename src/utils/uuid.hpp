#ifndef UUID_H
#define UUID_H
//#include "byte_crypto.hpp"
#include <cstring>
#include <random>
#include <sstream>

namespace cpp_streamer
{
class UUID
{
public:
    static uint32_t GetRandomUint(uint32_t min, uint32_t max) {
        std::random_device rd;  // a seed source for the random number engine
        std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<uint32_t> dest(min, max);
    
        return dest(gen);
    }   

    static std::string MakeUUID2() {
        std::stringstream ss;

        char uuid_sz[128];
    
        uint16_t data1 = GetRandomUint(0, 0xffff);
        uint16_t data2 = GetRandomUint(0, 0xffff);
        snprintf(uuid_sz, sizeof(uuid_sz), "%04x%04x", data1, data2);
        ss << uuid_sz;
        ss << "-";

        data1 = GetRandomUint(0, 0xffff);
        snprintf(uuid_sz, sizeof(uuid_sz), "%04x", data1);
        ss << uuid_sz;
        ss << "-";
        data1 = GetRandomUint(0, 0xffff);
        snprintf(uuid_sz, sizeof(uuid_sz), "%04x", data1);
        ss << uuid_sz;
        ss << "-";
        data1 = GetRandomUint(0, 0xffff);
        snprintf(uuid_sz, sizeof(uuid_sz), "%04x", data1);
        ss << uuid_sz;
        ss << "-";

        
        data1 = GetRandomUint(0, 0xffff);
        snprintf(uuid_sz, sizeof(uuid_sz), "%04x", data1);
        ss << uuid_sz;

        data1 = GetRandomUint(0, 0xffff);
        snprintf(uuid_sz, sizeof(uuid_sz), "%04x", data1);
        ss << uuid_sz;

        data1 = GetRandomUint(0, 0xffff);
        snprintf(uuid_sz, sizeof(uuid_sz), "%04x", data1);
        ss << uuid_sz;

        return ss.str();

    }

    static std::string MakeUUID() {
        char uuid_sz[128];
        int len = 0;
    
        for (size_t i = 0; i < 12; i++) {
            uint32_t data = GetRandomUint(0, 255);
            len += snprintf(uuid_sz + len, sizeof(uuid_sz), "%02x", data);
        }
        
        return std::string((char*)uuid_sz, strlen(uuid_sz));
    }

    static std::string MakeNumString(int count) {
        std::stringstream ss;
        for (int i = 0; i < count; i++) {
            int value = GetRandomUint(0, 9);
            ss << value%10;
        }
        return ss.str();
    }
};


}

#endif//UUID_H

