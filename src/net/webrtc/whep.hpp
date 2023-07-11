#ifndef WHEP_HPP
#define WHEP_HPP
#include "logger.hpp"
#include "http_client.hpp"
#include "peerconnection.hpp"
#include "cpp_streamer_interface.hpp"
#
extern "C" {
void* make_whep_streamer();
void destroy_whep_streamer(void* streamer);
}

namespace cpp_streamer
{

class Whep : public CppStreamerInterface, public HttpClientCallbackI, public PCStateReportI
{
public:
    Whep();
    virtual ~Whep();

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
    void ReleaseHttpClient();
    bool GetHostInfoByUrl(const std::string& url, std::string& host, 
            uint16_t& port, std::string& subpath, bool& https_enable);
    int Start(const std::string& host, uint16_t port, const std::string& subpath, bool https_enable);

private:
    Logger* logger_ = nullptr;
    std::string name_;

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
