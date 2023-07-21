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
#include "pack_handle_pub.hpp"
#include "media_callback_interface.hpp"

namespace cpp_streamer
{
typedef enum {
    MID_TYPE,
    RTP_STREAMID_TYPE,
    RP_RTP_STREAMID_TYPE,
    ABS_SEND_TIME_TYPE,
    TCC_WIDE_TYPE,
    AVTEXT_FRAMEMARKING_TYPE,
    RTP_HDREXT_FRAMEMARKING_TYPE,
    SSRC_AUDIO_LEVEL_TYPE,
    VIDEO_ORIENTATION_TYPE,
    TOFFSET_TYPE,
    ABS_CAPTURE_TIME_TYPE
} RTP_EXT_TYPE;

inline RTP_EXT_TYPE GetRtpExtType(const std::string& uri) {
    RTP_EXT_TYPE ret_type;
    if(uri == "urn:ietf:params:rtp-hdrext:sdes:mid") {
        ret_type = MID_TYPE;
    } else if (uri == "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id") {
        ret_type = RTP_STREAMID_TYPE;
    } else if (uri == "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id") {
        ret_type = RP_RTP_STREAMID_TYPE;
    } else if (uri == "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time") {
        ret_type = ABS_SEND_TIME_TYPE;
    } else if (uri == "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01") {
        ret_type = TCC_WIDE_TYPE;
    } else if (uri == "urn:ietf:params:rtp-hdrext:ssrc-audio-level") {
        ret_type = SSRC_AUDIO_LEVEL_TYPE;
    } else if (uri == "http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07") {
        ret_type = AVTEXT_FRAMEMARKING_TYPE;
    } else if (uri == "urn:ietf:params:rtp-hdrext:framemarking") {
        ret_type = RTP_HDREXT_FRAMEMARKING_TYPE;
    } else if (uri == "urn:3gpp:video-orientation") {
        ret_type = VIDEO_ORIENTATION_TYPE;
    } else if (uri == "urn:ietf:params:rtp-hdrext:toffset") {
        ret_type = TOFFSET_TYPE;
    } else if (uri == "http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time") {
        ret_type = ABS_CAPTURE_TIME_TYPE;
    } else {
        CSM_THROW_ERROR("unknown rtp ext type:%s", uri.c_str());
    }
    return ret_type;
}

typedef struct RTP_EXT_INFO_S {
    int id;
    RTP_EXT_TYPE type;
    std::string uri;
} RTP_EXT_INFO;

typedef enum {
    CC_UNKNOWN_TYPE,
    GCC_TYPE,
    TCC_TYPE
} CC_TYPE;

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

typedef enum {
    SDP_OFFER,
    SDP_ANSWER
} SDP_TYPE;

class PCStateReportI
{
public:
    virtual void OnState(const std::string& type, const std::string& value) = 0;
};

class PeerConnection : public UdpSessionCallbackI
    , public RtcSendStreamCallbackI
    , public TimerInterface
    , public JitterBufferCallbackI
    , public PackCallbackI
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
    int GetVideoMid(SDP_TYPE type);
    void SetVideoMid(SDP_TYPE type, int mid);

    int GetAudioMid(SDP_TYPE type);
    void SetAudioMid(SDP_TYPE type, int mid);

    void SetVideoSsrc(SDP_TYPE type, uint32_t ssrc);
    uint32_t GetVideoSsrc(SDP_TYPE type);

    void SetVideoRtxSsrc(SDP_TYPE type, uint32_t ssrc);
    uint32_t GetVideoRtxSsrc(SDP_TYPE type);

    void SetAudioSsrc(SDP_TYPE type, uint32_t ssrc);
    uint32_t GetAudioSsrc(SDP_TYPE type);

    void SetVideoPayloadType(SDP_TYPE type, int payloadType);
    int GetVideoPayloadType(SDP_TYPE type);

    void SetVideoRtxPayloadType(SDP_TYPE type, int payloadType);
    int GetVideoRtxPayloadType(SDP_TYPE type);

    void SetAudioPayloadType(SDP_TYPE type, int payloadType);
    int GetAudioPayloadType(SDP_TYPE type);

    std::string GetVideoCodecType(SDP_TYPE type);
    std::string GetAudioCodecType(SDP_TYPE type);

    bool GetVideoRtx(SDP_TYPE type);

    int GetVideoClockRate(SDP_TYPE type);
    void SetVideoClockRate(SDP_TYPE type, int clock_rate);

    int GetAudioClockRate(SDP_TYPE type);
    void SetAudioClockRate(SDP_TYPE type, int clock_rate);

    void SetVideoNack(SDP_TYPE type, bool enable, int payload_type);
    bool GetVideoNack(SDP_TYPE type, int payload_type);

    void SetCCType(SDP_TYPE type, CC_TYPE cc_type);
    CC_TYPE GetCCType(SDP_TYPE type);

    void AddRtpExtInfo(SDP_TYPE type, int id, const RTP_EXT_INFO& info);

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
    void CreateVideoRecvStream();
    void CreateAudioRecvStream();

public:
    void SetMediaCallback(MediaCallbackI* cb) { media_cb_ = cb; }

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

public:
    virtual void PackHandleReset(std::shared_ptr<RtpPacketInfo> pkt_ptr) override;
    virtual void MediaPacketOutput(std::shared_ptr<Media_Packet> pkt_ptr) override;

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
    void SendRr(int64_t now_ms);

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

private:
    PackHandleBase* h264_pack_    = nullptr;
    PackHandleBase* audio_pack_ = nullptr;

private:
    MediaCallbackI* media_cb_ = nullptr;

private:
    int64_t last_rr_ms_ = -1;

private://for rtp extern header
    std::map<int, RTP_EXT_INFO> rtp_ext_headers_;
};

}

#endif
