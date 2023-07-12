#ifndef PEER_CONNECTION_HPP
#define PEER_CONNECTION_HPP
#include "logger.hpp"
#include "dtls.hpp"
#include "sdp.hpp"
#include "udp_client.hpp"
#include "srtp_session.hpp"
#include "rtc_send_stream.hpp"
#include "rtc_recv_stream.hpp"
#include "jitterbuffer.hpp"
#include "timer.hpp"
#include "rtcp_xr_dlrr.hpp"

namespace cpp_streamer
{

typedef enum {
    DIRECTION_UNKNOWN,
    SEND_ONLY,
    RECV_ONLY,
    SEND_RECV
} WebRtcSdpDirection;

typedef enum {
    PC_INIT_STATE,
    PC_SDP_DONE_STATE,
    PC_STUN_DONE_STATE,
    PC_DTLS_DONE_STATE
} PC_STATE;

class PCStateReportI
{
public:
    virtual void OnState(const std::string& type, const std::string& value) = 0;
};

class PeerConnection : public UdpSessionCallbackI
    , public RtcSendStreamCallbackI
    , public TimerInterface
    , public JitterBufferCallbackI
{
public:
    PeerConnection(uv_loop_t* loop, Logger* logger, PCStateReportI* state_report);
    virtual ~PeerConnection();

public:
    std::string CreateOfferSdp(WebRtcSdpDirection direction_type);
    int ParseAnswerSdp(const std::string& sdp);
    int SendVideoPacket(Media_Packet_Ptr pkt_ptr);
    int SendAudioPacket(Media_Packet_Ptr pkt_ptr);

public:
    void SetRemoteIcePwd(const std::string& ice_pwd);
    void SetRemoteIceUserFrag(const std::string& user_frag);
    void SetRemoteUdpAddress(const std::string& ip, uint16_t port);
    void SetFingerPrintsSha256(const std::string& sha256_value);
    std::string GetFingerSha256();
    void UpdatePcState(PC_STATE pc_state);

public:
    int GetVideoMid();
    int GetAudioMid();
    uint32_t GetVideoSsrc();
    uint32_t GetVideoRtxSsrc();
    uint32_t GetAudioSsrc();
    int GetVideoPayloadType();
    int GetVideoRtxPayloadType();
    int GetAudioPayloadType();
    std::string GetVideoCodecType();
    std::string GetAudioCodecType();
    void SetVideoRtx(bool rtx);
    bool GetVideoRtx();
    int GetVideoClockRate();
    int GetAudioClockRate();
    std::vector<RtcpFbInfo> GetVideoRtcpFbInfo();
    std::vector<RtcpFbInfo> GetAudioRtcpFbInfo();
    void GetHeaderExternId(int& offset_id, int& abs_send_time_id,
            int& video_rotation_id, int& tcc_id,
            int& playout_delay_id, int& video_content_id,
            int& video_timing_id, int& color_space_id,
            int& sdes_id, int& rtp_streamid_id,
            int& rp_rtp_streamid_id, int& audio_level);
    std::string GetVideoCName();
    std::string GetAudioCName();
    void CreateSendStream2();

public:
    void OnDtlsConnected(CRYPTO_SUITE_ENUM suite,
                uint8_t* local_key, size_t local_key_len,
                uint8_t* remote_key, size_t remote_key_len);

//TimerInterface
protected:
    virtual void OnTimer() override;

protected:
    virtual void OnWrite(size_t sent_size, UdpTuple address) override;
    virtual void OnRead(const char* data, size_t data_size, UdpTuple address) override;

protected:
    virtual void SendRtpPacket(uint8_t* data, size_t len) override;
    virtual void SendRtcpPacket(uint8_t* data, size_t len) override;

public:
    virtual void RtpPacketReset(std::shared_ptr<RtpPacketInfo> pkt_ptr) override;
    virtual void RtpPacketOutput(std::shared_ptr<RtpPacketInfo> pkt_ptr) override;

private:
    std::string GetDirectionString(WebRtcSdpDirection direction_type);
    void Report(const std::string& key, const std::string& value);

private:
    void CreateSendStream();
    void CreateRecvStream();
    void OnStatics(int64_t now_ms);

private:
    void HandleRtpData(uint8_t* data, size_t len);

private:
    void HandleRtcp(uint8_t* data, size_t len);
    int HandleRtcpSr(uint8_t* data, int len);
    int HandleRtcpRr(uint8_t* data, int len);
    int HandleRtcpRtpFb(uint8_t* data, int len);
    int HandleRtcpPsFb(uint8_t* data, int len);
    int HandleRtcpXr(uint8_t* data, int len);
    int HandleXrDlrr(XrDlrrData* dlrr_block);

    void SendStun(int64_t now_ms);
    void SendXrDlrr(int64_t now_ms);

private:
    uv_loop_t* loop_ = nullptr;
    Logger* logger_ = nullptr;
    PCStateReportI* state_report_   = nullptr;
    WebRtcSdpDirection direct_type_ = DIRECTION_UNKNOWN;
    PC_STATE pc_state_ = PC_INIT_STATE;
    int64_t last_stun_ms_ = -1;

private:
    UdpClient* udp_client_ = nullptr;
    std::string pc_ipaddr_str_;
    uint16_t pc_udp_port_ = 0;
    RtcDtls dtls_;
    SdpTransform offer_sdp_;
    SdpTransform answer_sdp_;

    SRtpSession* write_srtp_ = nullptr;
    SRtpSession* read_srtp_  = nullptr;

private:
    bool has_rtx_ = false;
    RtcSendStream* video_send_stream_ = nullptr;
    RtcSendStream* audio_send_stream_ = nullptr;

private:
    RtcRecvStream* video_recv_stream_ = nullptr;
    RtcRecvStream* audio_recv_stream_ = nullptr;

private:
    NTP_TIMESTAMP last_xr_ntp_;
    int64_t last_xr_ms_ = -1;
    int64_t last_send_xr_dlrr_ms_ = -1;

private:
    int64_t last_statics_ms_ = -1;

private:
    JitterBuffer jb_video_;
    JitterBuffer jb_audio_;
};

}

#endif
