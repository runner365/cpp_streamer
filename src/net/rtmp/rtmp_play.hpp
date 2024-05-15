#ifndef RTMP_PLAY
#define RTMP_PLAY
#include "cpp_streamer_interface.hpp"
#include "rtmp_client_session.hpp"
#include "timeex.hpp"
#include "media_statics.hpp"

#include <string>
#include <map>
#include <uv.h>
#include <thread>
#include <memory>

extern "C" {
void* make_rtmpplay_streamer();
void destroy_rtmpplay_streamer(void* streamer);
}

namespace cpp_streamer
{

class RtmpPlay : public CppStreamerInterface, public RtmpClientDataCallbackI, public RtmpClientCtrlCallbackI
{
public:
    RtmpPlay();
    virtual ~RtmpPlay();

public:
    virtual std::string StreamerName() override;
    virtual void SetLogger(Logger* logger) override;
    virtual int AddSinker(CppStreamerInterface* sinker) override;
    virtual int RemoveSinker(const std::string& name) override;
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) override;
    virtual void StartNetwork(const std::string& url, void* loop_handle) override;
    virtual void AddOption(const std::string& key, const std::string& value) override;
    virtual void SetReporter(StreamerReport* reporter) override;

public: // RtmpClientDataCallbackI
    virtual void OnMessage(int ret_code, Media_Packet_Ptr pkt_ptr) override;

public: // RtmpClientCtrlCallbackI
    virtual void OnRtmpHandShakeSendC0C1(int ret_code, uint8_t* data, size_t len) override;
    virtual void OnRtmpHandShakeRecvS0S1S2(int ret_code, uint8_t* data, size_t len) override;
    virtual void OnRtmpConnectSend(int ret_code,
        const std::map<std::string, std::string>& items) override;
    virtual void OnRtmpConnectRecv(
        int ret,
        const std::string& result,
        int64_t transaction_id,
        const std::map<std::string, std::string>& items) override;
    
    virtual void OnRtmpChunkSize(
        int ret,
        uint32_t chunk_size) override;
    virtual void OnRtmpWindowSize(
        int ret,
        uint32_t window_size) override;
    virtual void OnRtmpBandWidth(
        int ret,
        uint32_t bandwidth) override;
    virtual void OnRtmpCtrlAck() override;
    virtual void OnRtmpCreateStreamSend(
        int ret,
        int64_t transaction_id) override;
    virtual void OnRtmpCreateStreamRecv(
        int ret,
        const std::string& result,
        int64_t transaction_id,
        int64_t stream_id,
        const std::map<std::string, std::string>& items) override;

    virtual void OnRtmpPlayPublishSend(
        const std::string& oper,//play or publish
        int64_t transaction_id,
        const std::string stream_name) override;
    virtual void OnRtmpPlayPublishRecv(
        int ret,
        const std::string& status,
        int64_t transaction_id,
        const std::map<std::string, std::string>& items) override;
    virtual void OnClose(int ret_code) override;

private:
    void OnWork();
    void Init();
    void Release();
    void ReportEvent(const std::string& type, const std::string& value);
    void ReportStatics();
    void ReportMetaData(uint8_t* data, size_t len);

private:
    std::string src_url_;

private:
    uv_loop_t* loop_ = nullptr;
    std::shared_ptr<std::thread> thread_ptr_;
    bool running_ = false;

private:
    RtmpClientSession* client_session_ = nullptr;
    MediaStatics statics_;
    int64_t rpt_ts_ = -1;

private:
    uint8_t sps_[512];
    uint8_t pps_[512];
    size_t sps_len_ = 0;
    size_t pps_len_ = 0;

private:
    Logger* logger_ = nullptr;
};
}

#endif
