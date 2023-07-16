#ifndef MEDIA_CALLBACK_INTERFACE_HPP
#define MEDIA_CALLBACK_INTERFACE_HPP
#include "media_packet.hpp"

namespace cpp_streamer
{

class MediaCallbackI
{
public:
    virtual void OnReceiveMediaPacket(Media_Packet_Ptr pkt_ptr) = 0;
};

}

#endif
