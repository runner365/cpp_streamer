#ifndef WHIP_HPP
#define WHIP_HPP
#include "logger.hpp"
#include "http_client.hpp"
#include "peerconnection.hpp"
#include "cpp_streamer_interface.hpp"
#include <queue>
#include <mutex>

extern "C" {
void* make_whip_streamer();
void destroy_whip_streamer(void* streamer);
}

namespace cpp_streamer
{

class Whip : public CppStreamerInterface, public HttpClientCallbackI, public PCStateReportI
{
friend void SourceWhipData(uv_async_t *handle);

public:
    Whip();
    virtual ~Whip();

public:
    virtual std::string StreamerName() override;
    virtual void SetLogger(Logger* logger) override;
    virtual int AddSinker(CppStreamerInterface* sinker) override;
    virtual int RemoveSinker(const std::string& name) override;
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) override;
    virtual void StartNetwork(const std::string& url, void* loop_handle) override;
    virtual void AddOption(const std::string& key, const std::string& value) override;
    virtual void SetReporter(StreamerReport* reporter) override;

protected:
    virtual void OnHttpRead(int ret, std::shared_ptr<HttpClientResponse> resp_ptr) override;

public:
    virtual void OnState(const std::string& type, const std::string& value) override;

private:
    Media_Packet_Ptr GetMediaPacket();
    void HandleMediaData();

private:
    void ReleaseHttpClient();
    bool GetHostInfoByUrl(const std::string& url, std::string& host, 
            uint16_t& port, std::string& subpath);
    int Start(const std::string& host, uint16_t port, const std::string& subpath);

private:
    Logger* logger_ = nullptr;
    std::string name_;

private:
    std::queue<Media_Packet_Ptr> packet_queue_;
    std::mutex mutex_;
    uv_async_t async_;

private:
    HttpClient* hc_ = nullptr;
    uv_loop_t* loop_ = nullptr;
    PeerConnection* pc_ = nullptr;

private:
    std::string host_;
    uint16_t port_;
    std::string subpath_;
    int64_t start_ms_ = -1;
};

}

#endif
