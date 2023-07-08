#ifndef UDP_SERVER_HPP
#define UDP_SERVER_HPP
#include "udp_pub.hpp"

namespace cpp_streamer
{

class UdpServer : public UdpSessionBase
{
public:
    UdpServer(uv_loop_t* loop, 
            uint16_t port, 
            UdpSessionCallbackI* cb, 
            Logger* logger):UdpSessionBase(loop, 
                                        cb, 
                                        logger)
    {
        uv_udp_init(loop, &udp_handle_);
        struct sockaddr_in recv_addr;
        uv_ip4_addr("0.0.0.0", port, &recv_addr);
        uv_udp_bind(&udp_handle_, (const struct sockaddr *)&recv_addr, UV_UDP_REUSEADDR);
        udp_handle_.data = this;

        TryRead();
    }

    ~UdpServer() {
    }
};

}
#endif
