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

class RtmpPlay : public CppStreamerInterface, public RtmpClientCallbackI
{
public:
    RtmpPlay();
    virtual ~RtmpPlay();

public:
    virtual std::string StreamerName();
    virtual void SetLogger(Logger* logger);
    virtual int AddSinker(CppStreamerInterface* sinker);
    virtual int RemoveSinker(const std::string& name);
    virtual int SourceData(Media_Packet_Ptr pkt_ptr);
    virtual void StartNetwork(const std::string& url, void* loop_handle);
    virtual void AddOption(const std::string& key, const std::string& value);
    virtual void SetReporter(StreamerReport* reporter);

public:
    virtual void OnMessage(int ret_code, Media_Packet_Ptr pkt_ptr);
    virtual void OnRtmpHandShake(int ret_code);
    virtual void OnRtmpConnect(int ret_code);
    virtual void OnRtmpCreateStream(int ret_code);
    virtual void OnRtmpPlayPublish(int ret_code);
    virtual void OnClose(int ret_code);

private:
    void OnWork();
    void Init();
    void Release();
    void ReportEvent(const std::string& type, const std::string& value);
    void ReportStatics();

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
    Logger* logger_ = nullptr;
};
}

#endif
