#ifndef SDP_HPP
#define SDP_HPP
#include "logger.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <map>
#include <vector>

namespace cpp_streamer
{

#define DEF_VERSION "0"

#define VPLAYLOAD_DEF_TYPE   106
#define RTX_PAYLOAD_DEF_TYPE 107
#define APLAYLOAD_DEF_TYPE   111

typedef struct {
    int payload_type;
    std::string codec_type;
    int clock_rate;
    int channel;
} RtpMapInfo;

typedef struct {
    int payload_type;
    std::string attr_string;
    bool is_video;
    bool is_rtx;
    int rtx_payload_type;
} FmtpInfo;

typedef struct {
    int payload_type;
    std::string attr_string;
} RtcpFbInfo;

typedef struct {
    int ext_id;
    std::string desc;
} ExtInfo;

typedef struct {
    uint32_t ssrc;
    bool is_video;
    bool is_rtx;
    std::string msid;
    std::string msid_appdata;
    std::string cname;
    uint32_t rtx_ssrc;
} SSRCInfo;

class RtcDtls;
class SdpTransform
{
public:
    SdpTransform(RtcDtls* dtls, Logger* logger);
    ~SdpTransform();

public:
    std::string GenSdpString(bool has_video, bool has_audio);
    std::string GenSdpString();
    int Parse(const std::string& sdp);

    int GetVideoPayloadType();
    int GetVideoRtxPayloadType();
    int GetAudioPayloadType();

    uint32_t GetVideoSsrc();
    uint32_t GetAudioSsrc();
    uint32_t GetVideoRtxSsrc();

    bool IsVideoNackEnable();
    bool IsAudioNackEnable();
    bool IsVideoRtxEnable();
    void SetVideoRtxFlag(bool flag);

    int GetVideoClockRate();
    int GetVideoRtxClockRate();
    int GetAudioClockRate();

private:
    int ParseLine(std::string line);
    int ParseM(const std::string& line);
    int ParseRtpMap(const std::string& line);
    int ParseFmtp(const std::string& line);
    int ParseRtcpFb(const std::string& line);
    int ParseExtMap(const std::string& line);
    int ParseSsrcInfo(const std::string& line);
    int ParseSsrcGroupFid(const std::string& line);

private:
    std::string GetProtoVersion();
    std::string GetSessionIdentifier();
    std::string GetSessionName();
    std::string GetTimeScale(int64_t start = 0, int64_t end = 0);
    std::string GetBasicAttr(bool has_video, bool has_audio);

    std::string GetVideoDesc();
    std::string GetAudioDesc();

    std::string GetMediaDesc(bool is_video);
    std::string GetConnection();
    std::string GetRtpMap(bool is_video);
    std::string GetFmtp(bool is_video);
    std::string GetRtcp(bool is_video);
    std::string GetExtMap(bool is_video);
    std::string GetMidInfo(bool is_video);
    std::string GetIceInfo(RtcDtls* dtls);
    std::string GetSsrcs(bool is_video);

public:
    Logger* logger_ = nullptr;
    RtcDtls* dtls_ = nullptr;
    std::string proto_ver_ = DEF_VERSION;
    std::string session_name_;
    std::string session_id_;
    bool has_video_ = false;
    bool has_audio_ = false;
    std::string v_msid_;
    std::string a_msid_;
    std::string v_msid_appdata_;
    std::string a_msid_appdata_;

public:
    /*
    int vpayload_type_ = 0;
    int vrtx_type_     = 0;
    int apayload_type_ = 0;
    */

    std::vector<int> video_pt_vec_;
    std::vector<int> audio_pt_vec_;

    std::map<int, RtpMapInfo> video_rtp_map_infos_;
    std::map<int, RtpMapInfo> audio_rtp_map_infos_;
    std::vector<FmtpInfo> video_fmtp_vec_;
    std::vector<FmtpInfo> audio_fmtp_vec_;
    std::vector<RtcpFbInfo> video_rtcpfb_vec_;
    std::vector<RtcpFbInfo> audio_rtcpfb_vec_;
    std::map<int, ExtInfo> ext_map_;
    std::map<uint32_t, SSRCInfo> ssrc_info_map_;
    std::vector<uint32_t> video_fid_vec_;
    std::vector<uint32_t> audio_fid_vec_;

public:
    int level_asym_allowed_ = 1;
    int packet_mode_ = 1;
    std::string profile_level_id_ = "42e01f";
    int start_kbitrate_ = 600;
    int min_kbitrate_ = 300;
    int max_kbitrate_ = 1200;

public:
    int minptime_ = 10;
    int useinbandfec_ = 1;

public:
    int em_toffset_         = -1;
    int em_abs_send_time_   = -1;
    int em_video_rotation_  = -1;
    int em_tcc_             = -1;
    int em_playout_delay_   = -1;
    int em_video_content_   = -1;
    int em_video_timing_    = -1;
    int em_color_space_     = -1;
    int em_sdes_            = -1;
    int em_rtp_streamid_    = -1;
    int em_rp_rtp_streamid_ = -1;

public:
    int em_audio_level_    = -1;

public:
    std::string setup_ = "passive";
    int vmid_ = -1;
    int amid_ = -1;
    std::string direction_ = "sendonly";

public:
    uint32_t video_ssrc_      = 0;
    uint32_t video_rtx_ssrc_  = 0;
    uint32_t audio_ssrc_      = 0;
    int video_clock_rate_     = 90000;
    int video_rtx_clock_rate_ = 90000;
    int audio_clock_rate_     = 48000;
    int channel_              = 2;

    std::string video_cname_;
    std::string audio_cname_;

private:
    bool current_is_video_ = true;
    bool rtx_enable_   = false;
};

}

#endif

