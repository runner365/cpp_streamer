#ifndef RTMP_CLIENT_SESSION_HPP
#define RTMP_CLIENT_SESSION_HPP
#include "tcp_client.hpp"
#include "tcp_pub.hpp"
#include "rtmp_pub.hpp"
#include "rtmp_handshake.hpp"
#include "rtmp_session_base.hpp"
#include "data_buffer.hpp"
#include "media_packet.hpp"
#include "logger.hpp"

#include <uv.h>
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <vector>
#include <string>

namespace cpp_streamer
{

class RtmpClientCallbackI
{
public:
    virtual void OnMessage(int ret_code, Media_Packet_Ptr pkt_ptr) = 0;
    virtual void OnRtmpHandShake(int ret_code) = 0;
    virtual void OnRtmpConnect(int ret_code) = 0;
    virtual void OnRtmpCreateStream(int ret_code) = 0;
    virtual void OnRtmpPlayPublish(int ret_code) = 0;
    virtual void OnClose(int ret_code) = 0;
};

class RtmpControlHandler;
class RtmpClientHandshake;
class RtmpWriter;

class RtmpClientSession : public TcpClientCallback, public RtmpSessionBase
{
friend class RtmpControlHandler;
friend class RtmpClientHandshake;
friend class RtmpWriter;

public:
    RtmpClientSession(uv_loop_t* loop,
            RtmpClientCallbackI* callback,
            Logger* logger = nullptr);
    virtual ~RtmpClientSession();

public://tcp client callback implement
    virtual void OnConnect(int ret_code) override;
    virtual void OnWrite(int ret_code, size_t sent_size) override;
    virtual void OnRead(int ret_code, const char* data, size_t data_size) override;

public:
    void TryRead();
    void Close();
    bool IsReady();

public:
    int Start(const std::string& url, bool is_publish);
    int RtmpWrite(Media_Packet_Ptr pkt_ptr);

protected://implement rtmp_session_base
    DataBuffer* GetRecvBuffer() override;
    int RtmpSend(char* data, int len) override;
    int RtmpSend(std::shared_ptr<DataBuffer> data_ptr) override;

private://rtmp client behavior
    int RtmpConnect();
    int RtmpCreatestream();
    int RtmpPlay();
    int RtmpPublish();
    int ReceiveRespMessage();
    int HandleMessage();

private:
    TcpClient conn_;
    RtmpClientCallbackI* cb_;
    RtmpClientHandshake hs_;

private:
    std::string host_;
    uint16_t    port_ = 1935;

private:
    RtmpControlHandler ctrl_handler_;

private:
    Logger* logger_ = nullptr;
};

}
#endif
