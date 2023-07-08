#ifndef STRING_EXTEN_HPP
#define STRING_EXTEN_HPP
#include <random>
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <algorithm>

namespace cpp_streamer
{

inline int StringSplit(const std::string& input_str, const std::string& split_str, std::vector<std::string>& output_vec) {
    if (input_str.length() == 0) {
        return 0;
    }
    
    std::string tempString(input_str);
    do {

        size_t pos = tempString.find(split_str);
        if (pos == tempString.npos) {
            output_vec.push_back(tempString);
            break;
        }
        std::string seg_str = tempString.substr(0, pos);
        tempString = tempString.substr(pos+split_str.size());
        output_vec.push_back(seg_str);
    } while(tempString.size() > 0);

    return output_vec.size();
}

inline std::string DataToString(uint8_t* data, size_t len) {
    char print_data[4*1024];
    size_t print_len = 0;
    const int max_print = 512;

    for (size_t index = 0; index < (len > max_print ? max_print : len); index++) {
        if ((index%16) == 0) {
            print_len += snprintf(print_data + print_len, sizeof(print_data) - print_len, "\r\n");
        }
        
        print_len += snprintf(print_data + print_len, sizeof(print_data) - print_len,
            " %02x", data[index]);
    }
    return std::string(print_data);
}

inline void String2Lower(std::string& data) {
    for (auto& c : data) {
        c = ::tolower(c);
    }
}

inline void String2Upper(std::string& data) {
    for (auto& c : data) {
        c = ::toupper(c);
    }
}

inline void RemoveSubfix(std::string& data, const std::string& subfix) {
    size_t pos = data.rfind(subfix);
    if (pos == data.npos) {
        return;
    }
    if ((data.length() - pos) == subfix.length()) {
        data = data.substr(0, pos);
    }
}
}
#endif //STRING_EXTEN_HPP
