#ifndef IP_ADDRESS_HPP
#define IP_ADDRESS_HPP
#include <arpa/inet.h>  // htonl(), htons(), ntohl(), ntohs()
#include <netinet/in.h> // sockaddr_in, sockaddr_in6
#include <sys/socket.h> // struct sockaddr, struct sockaddr_storage, AF_INET, AF_INET6
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <cstring>

namespace cpp_streamer
{
    inline std::string GetIpStr(const struct sockaddr *sa, uint16_t& port) {
        const socklen_t maxlen = 64;
        char s[maxlen];

        if (!sa) {
            return "";
        }

        std::memset(s, 0, maxlen);
        switch(sa->sa_family) {
            case AF_INET:
                inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                        s, maxlen);
                port = ((struct sockaddr_in *)sa)->sin_port;
                break;

            case AF_INET6:
                inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                        s, maxlen);
                port = ((struct sockaddr_in *)sa)->sin_port;
                break;
        }

        return std::string(s);
    }

    inline void GetIpv4Sockaddr(const std::string& ip, uint16_t port, struct sockaddr* addr) {
        struct sockaddr_in* sa = (struct sockaddr_in*)addr;

        inet_pton(AF_INET, ip.c_str(), &(sa->sin_addr));
        sa->sin_port = port;
        sa->sin_family = AF_INET;
        return;
    }

    inline bool IsIPv4(const std::string& IP)
    {
        int dotcnt = 0;
        //数一共有几个.
        for (size_t i = 0; i < IP.length(); i++)
        {
            if (IP[i] == '.')
                dotcnt++;
        }
        //ipv4地址一定有3个点
        if (dotcnt != 3)
            return false;
        std::string temp = "";
        for (size_t i = 0; i < IP.length(); i++)
        {
            if (IP[i] != '.')
                temp += IP[i];
            //被.分割的每部分一定是数字0-255的数字
            if (IP[i] == '.' || i == IP.length() - 1)
            {
                if (temp.length() == 0 || temp.length() > 3)
                    return false;
                for (size_t j = 0; j < temp.length(); j++)
                {
                    if (!isdigit(temp[j]))
                        return false;
                }
                int tempInt = stoi(temp);
                if (tempInt > 255 || tempInt < 0)
                    return false;
                std::string convertString = std::to_string(tempInt);
                if (convertString != temp)
                    return false;
                temp = "";
            }
        }
        if (IP[IP.length()-1] == '.')
            return false;
        return true;
    }
}
#endif
