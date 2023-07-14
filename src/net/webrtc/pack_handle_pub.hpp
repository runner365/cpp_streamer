#ifndef PACK_HANDLE_PUB_HPP
#define PACK_HANDLE_PUB_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <memory>
#include "jitterbuffer_pub.hpp"
#include "utils/av/media_packet.hpp"

namespace cpp_streamer
{

#define PACK_BUFFER_TIMEOUT 600 //ms

class PackCallbackI
{
public:
    virtual void PackHandleReset(std::shared_ptr<RtpPacketInfo> pkt_ptr) = 0;
    virtual void MediaPacketOutput(std::shared_ptr<Media_Packet> pkt_ptr) = 0;
};

class PackHandleBase
{
public:
    PackHandleBase()
    {
    }
    virtual ~PackHandleBase()
    {
    }
    
public:
    virtual void InputRtpPacket(std::shared_ptr<RtpPacketInfo> pkt_ptr) = 0;
};

}
#endif

