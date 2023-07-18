#ifndef MS_PULL_HPP
#define MS_PULL_HPP
#include "logger.hpp"
#include "http_client.hpp"
#include "peerconnection.hpp"
#include "cpp_streamer_interface.hpp"
#include "json.hpp"
#
extern "C" {
void* make_mspull_streamer();
void destroy_mspull_streamer(void* streamer);
}

namespace cpp_streamer
{
typedef enum
{
    BROADCASTER_INIT,
    BROADCASTER_REQUEST,
    BROADCASTER_TRANSPORT,
    BROADCASTER_TRANSPORT_CONNECT,
    BROADCASTER_VIDEO_CONSUME,
    BROADCASTER_AUDIO_CONSUME,
    BROADCASTER_DONE
} BROADCASTER_STATE;


class MsPull : public CppStreamerInterface , public HttpClientCallbackI, public PCStateReportI
{
public:
    MsPull();
    virtual ~MsPull();

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
    void ReleaseHttpClient(HttpClient*& hc);
    bool GetHostInfoByUrl(const std::string& url, 
            std::string& host, 
            uint16_t& port, 
            std::string& roomId, 
            std::string& userId,
            std::string& vProduceId,
            std::string& aProduceId);
    void BroadCasterRequest();
    void TransportRequest();
    void TransportConnectRequest();
    void VideoConsumeRequest();
    void AudioConsumeRequest();
    void HandleBroadCasterResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
    void HandleTransportResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
    void HandleTransportConnectResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
    void HandleVideoConsumeResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
    void HandleAudioConsumeResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
 
private:
    void Report(const std::string& type, const std::string& value);

private:
    Logger* logger_ = nullptr;
    std::string name_;

private:
    HttpClient* hc_req_           = nullptr;
    HttpClient* hc_transport_     = nullptr;
    HttpClient* hc_trans_connect_ = nullptr;
    HttpClient* hc_video_consume_ = nullptr;
    HttpClient* hc_audio_consume_ = nullptr;
    uv_loop_t* loop_    = nullptr;
    PeerConnection* pc_ = nullptr;

private:
    std::string host_;
    uint16_t port_;
    std::string roomId_;
    std::string userId_;
    BROADCASTER_STATE state_ = BROADCASTER_INIT;
    int64_t start_ms_ = -1;

private:
    std::string transport_id_;
    std::string video_produce_id_;
    std::string audio_produce_id_;

private:
    std::string ice_pwd_;
    std::string ice_usr_frag_;
    std::string candidate_ip_;
    int candidate_port_;
    std::string candidate_proto_;
    std::string alg_value_;
};

}

#endif

