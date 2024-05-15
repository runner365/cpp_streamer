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

class RtmpClientDataCallbackI
{
public:
    virtual void OnMessage(int ret_code, Media_Packet_Ptr pkt_ptr) = 0;
};

class RtmpClientCtrlCallbackI
{
public:
    virtual void OnRtmpHandShakeSendC0C1(int ret_code, uint8_t* data, size_t len) = 0;
    virtual void OnRtmpHandShakeRecvS0S1S2(int ret_code, uint8_t* data, size_t len) = 0;
    virtual void OnRtmpConnectSend(int ret_code,
        const std::map<std::string, std::string>& items) = 0;
    virtual void OnRtmpConnectRecv(
        int ret,
        const std::string& result,
        int64_t transaction_id,
        const std::map<std::string, std::string>& items) = 0;
    
    virtual void OnRtmpChunkSize(
        int ret,
        uint32_t chunk_size) = 0;
    virtual void OnRtmpWindowSize(
        int ret,
        uint32_t window_size) = 0;
    virtual void OnRtmpBandWidth(
        int ret,
        uint32_t bandwidth) = 0;
    virtual void OnRtmpCtrlAck() = 0;

    virtual void OnRtmpCreateStreamSend(
        int ret,
        int64_t transaction_id) = 0;
    virtual void OnRtmpCreateStreamRecv(
        int ret,
        const std::string& result,
        int64_t transaction_id,
        int64_t stream_id,
        const std::map<std::string, std::string>& items) = 0;

    virtual void OnRtmpPlayPublishSend(
        const std::string& oper,//play or publish
        int64_t transaction_id,
        const std::string stream_name) = 0;

    virtual void OnRtmpPlayPublishRecv(
        int ret,
        const std::string& status,
        int64_t transaction_id,
        const std::map<std::string, std::string>& items) = 0;
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
            RtmpClientDataCallbackI* data_cb,
            RtmpClientCtrlCallbackI* ctrl_cb,
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
    void getItemMap(AMF_ITERM* item, std::map<std::string, std::string>& items);
    void reportConnectRespAmf(int ret, const std::vector<AMF_ITERM*>& amf_vec);
    void reportCreateStreamRespAmf(int ret, const std::vector<AMF_ITERM*>& amf_vec);
    void reportPlayPublishRespAmf(int ret, const std::vector<AMF_ITERM*>& amf_vec);
    void reportCtrlMsg(CHUNK_STREAM_PTR cs_ptr);

private:
    TcpClient conn_;
    RtmpClientDataCallbackI* data_cb_ = nullptr;
    RtmpClientCtrlCallbackI* ctrl_cb_ = nullptr;
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
