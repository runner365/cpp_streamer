#ifndef RTMP_PUBLISH_HPP
#define RTMP_PUBLISH_HPP
#include "cpp_streamer_interface.hpp"
#include "rtmp_client_session.hpp"
#include "timeex.hpp"
#include "media_statics.hpp"

#include <string>
#include <map>
#include <uv.h>
#include <thread>
#include <memory>
#include <queue>
#include <mutex>

extern "C" {
void* make_rtmppublish_streamer();
void destroy_rtmppublish_streamer(void* streamer);
}

namespace cpp_streamer
{

extern "C" {
void* make_rtmppublish_streamer();
void destroy_rtmppublish_streamer(void* streamer);
}

class RtmpPublish : public CppStreamerInterface, public RtmpClientCallbackI
{
friend void SourceRtmpData(uv_async_t *handle);

public:
    RtmpPublish();
    virtual ~RtmpPublish();

public:
    virtual std::string StreamerName() override;
    virtual void SetLogger(Logger* logger) override;
    virtual int AddSinker(CppStreamerInterface* sinker) override;
    virtual int RemoveSinker(const std::string& name) override;
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) override;
    virtual void StartNetwork(const std::string& url, void* loop_handle) override;
    virtual void AddOption(const std::string& key, const std::string& value) override;
    virtual void SetReporter(StreamerReport* reporter) override;

public:
    virtual void OnMessage(int ret_code, Media_Packet_Ptr pkt_ptr) override;
    virtual void OnRtmpHandShake(int ret_code) override;
    virtual void OnRtmpConnect(int ret_code) override;
    virtual void OnRtmpCreateStream(int ret_code) override;
    virtual void OnRtmpPlayPublish(int ret_code) override;
    virtual void OnClose(int ret_code) override;

private:
    void OnWork();
    void Init();
    void Release();
    void ReportEvent(const std::string& type, const std::string& value);
    void ReportStatics();

    Media_Packet_Ptr GetMediaPacket();
    void HandleMediaData();
    void HandleVideoData(Media_Packet_Ptr pkt_ptr);
    void HandleAudioData(Media_Packet_Ptr pkt_ptr);
    void SendVideo(Media_Packet_Ptr pkt_ptr);
    void SendRtmp(Media_Packet_Ptr pkt_ptr);

private:
    std::string src_url_;

private:
    uv_loop_t* loop_ = nullptr;
    std::shared_ptr<std::thread> thread_ptr_;
    bool running_ = false;
    bool ready_   = false;

private:
    std::queue<Media_Packet_Ptr> packet_queue_;
    std::mutex mutex_;
    uv_async_t async_;

private:
    RtmpClientSession* client_session_ = nullptr;
    MediaStatics statics_;
    int64_t rpt_ts_ = -1;

private:
    Logger* logger_ = nullptr;

private:
    bool first_video_ = false;
    uint8_t sps_[512];
    uint8_t pps_[512];
    int sps_len_ = -1;
    int pps_len_ = -1;
};

}

#endif
