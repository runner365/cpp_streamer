#include "sdp.hpp"
#include "uuid.hpp"
#include "dtls.hpp"
#include "stringex.hpp"

#include <sstream>

namespace cpp_streamer
{


SdpTransform::SdpTransform(RtcDtls* dtls, Logger* logger):logger_(logger)
                                                        , dtls_(dtls)
{
}

SdpTransform::~SdpTransform()
{
}

uint32_t SdpTransform::GetVideoSsrc() {
    if (ssrc_info_map_.empty()) {
        return 0;
    }
    for (auto item : ssrc_info_map_) {
        if (item.second.is_video && !item.second.is_rtx) {
            return item.first;
        }
    }
    return 0;
}

uint32_t SdpTransform::GetAudioSsrc() {
    if (ssrc_info_map_.empty()) {
        return 0;
    }
    for (auto item : ssrc_info_map_) {
        if (!item.second.is_video) {
            return item.first;
        }
    }
    return 0;

}

uint32_t SdpTransform::GetVideoRtxSsrc() {
    if (ssrc_info_map_.empty()) {
        return 0;
    }
    for (auto item : ssrc_info_map_) {
        if (item.second.is_video && item.second.is_rtx) {
            return item.first;
        }
    }
    return 0;
}

std::string SdpTransform::GenSdpString() {
    std::stringstream ss;
    
    ss << GetProtoVersion();
    ss << GetSessionIdentifier();
    ss << GetSessionName();
    ss << GetTimeScale(0, 0);
    ss << GetBasicAttr(has_video_, has_audio_);
    ss << GetVideoDesc();
    ss << GetAudioDesc();

    return ss.str();
}

std::string SdpTransform::GenSdpString(bool has_video, bool has_audio) {
    std::stringstream ss;
    
    ss << GetProtoVersion();
    ss << GetSessionIdentifier();
    ss << GetSessionName();
    ss << GetTimeScale(0, 0);
    ss << GetBasicAttr(has_video, has_audio);
    ss << GetVideoDesc();
    ss << GetAudioDesc();

    return ss.str();
}

int SdpTransform::Parse(const std::string& sdp) {
    std::vector<std::string> sdp_lines;
    StringSplit(sdp, "\n", sdp_lines);

    for (const std::string& line : sdp_lines) {
        ParseLine(line);
    }
    return 0;
}

//eg. m=video 9 UDP/TLS/RTP/SAVPF 106 107
//eg. m=audio 9 UDP/TLS/RTP/SAVPF 111
int SdpTransform::ParseM(const std::string& line) {
    size_t pos = line.find("=");

    if (pos == std::string::npos) {
        LogErrorf(logger_, "m= attr error:%s", line.c_str());
        return -1;
    }
    std::string m_attr = line.substr(pos + 1);
    std::vector<std::string> attr_vec;

    StringSplit(m_attr, " ", attr_vec);
    if (attr_vec.size() < 4) {
        LogErrorf(logger_, "m= attr error:%s, attr count:%lu", 
                line.c_str(), attr_vec.size());
        return -1;
    }
    current_is_video_ = (attr_vec[0] == "video");
    for (size_t i = 3; i < attr_vec.size(); i++) {
        int pt = atoi(attr_vec[i].c_str());
        if (current_is_video_) {
            has_video_ = true;
            video_pt_vec_.push_back(pt);
        } else {
            has_audio_ = true;
            audio_pt_vec_.push_back(pt);
        }
    }

    return 0;
}

//a=rtpmap:106 H264/90000
//a=rtpmap:107 rtx/90000
//a=rtpmap:111 opus/48000/2
int SdpTransform::ParseRtpMap(const std::string& line) {
    const std::string rtpmap_attr("a=rtpmap");

    std::string rtp_map_str = line.substr(rtpmap_attr.size() + 1);

    size_t pos = rtp_map_str.find(" ");
    int pt = atoi(rtp_map_str.substr(0, pos).c_str());

    std::string codec_clock_str = rtp_map_str.substr(pos + 1);
    std::vector<std::string> items;

    StringSplit(codec_clock_str, "/", items);
    if (items.size() < 2) {
        LogErrorf(logger_, "rtpmap error:%s, codec clock vector size:%lu",
                line.c_str(), items.size());
        return -1;
    }
    RtpMapInfo info;

    info.payload_type = pt;
    info.codec_type   = items[0];
    info.clock_rate   = atoi(items[1].c_str());

    if (items.size() > 2) {
        info.channel = atoi(items[2].c_str());
    } else {
        info.channel = 0;
    }

    if (current_is_video_) {
        video_rtp_map_infos_[pt] = info;
        if (info.codec_type == "rtx") {
            video_rtx_clock_rate_ = info.clock_rate;
            LogInfof(logger_, "video rtx clock rate:%d", video_rtx_clock_rate_);
        } else {
            video_clock_rate_ = info.clock_rate;
            LogInfof(logger_, "video  clock rate:%d", video_clock_rate_);
        }
    } else {
        audio_rtp_map_infos_[pt] = info;
        audio_clock_rate_ = info.clock_rate;
        channel_ = info.channel;
        LogInfof(logger_, "audio  clock rate:%d, channel:%d", audio_clock_rate_, channel_);
    }

    return 0;
}

//a=fmtp:106 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
//a=fmtp:107 apt=106
//a=fmtp:111 minptime=10;useinbandfec=1
int SdpTransform::ParseFmtp(const std::string& line) {
    const std::string fmtp_attr("a=fmtp");
    const std::string apt_attr("apt");

    std::string fmtp_str = line.substr(fmtp_attr.length() + 1);
    size_t pos = fmtp_str.find(" ");

    if (pos == std::string::npos) {
        LogErrorf(logger_, "fmtp attr error:%s", line.c_str());
        return -1;
    }
    int pt = atoi(fmtp_str.substr(0, pos).c_str());
    std::string attr = fmtp_str.substr(pos + 1);

    FmtpInfo info = {
        .payload_type = pt,
        .attr_string  = attr
    };

    pos = attr.find(apt_attr);
    if (pos != std::string::npos) {
        info.is_rtx = true;
        std::string rtx_type_str = attr.substr(pos + apt_attr.length() + 1);
        info.rtx_payload_type = atoi(rtx_type_str.c_str());
    } else {
        info.is_rtx = false;
    }
    if (current_is_video_) {
        info.is_video = true;
    } else {
        info.is_video = false;
    }

    LogInfof(logger_, "fmtp current is %s, pt:%d, attr:%s, is_rtx:%d, pos:%lu",
            current_is_video_ ? "video" : "audio", 
            pt, attr.c_str(),
            info.is_rtx, pos);
    if (current_is_video_) {
        video_fmtp_vec_.push_back(info);
    } else {
        audio_fmtp_vec_.push_back(info);
    }
    return 0;
}
/*
a=rtcp-fb:106 goog-remb
a=rtcp-fb:106 transport-cc
a=rtcp-fb:106 cmm fir
a=rtcp-fb:106 nack
a=rtcp-fb:106 nack pli
 */
int SdpTransform::ParseRtcpFb(const std::string& line) {
    const std::string rtcp_fb_attr("a=rtcp-fb");
    std::string rtcp_fb_string = line.substr(rtcp_fb_attr.length() + 1);

    size_t pos = rtcp_fb_string.find(" ");
    if (pos == std::string::npos) {
        LogErrorf(logger_, "rtcp fb error:%s", line.c_str());
        return -1;
    }
    int pt = atoi(rtcp_fb_string.substr(0, pos).c_str());
    std::string attr_string = rtcp_fb_string.substr(pos + 1);

    RtcpFbInfo info = {
        .payload_type = pt,
        .attr_string  = attr_string
    };

    if (current_is_video_) {
        video_rtcpfb_vec_.push_back(info);
    } else {
        audio_rtcpfb_vec_.push_back(info);
    }

    return 0;
}

bool SdpTransform::IsVideoNackEnable() {
    for (auto& rtcpfb : video_rtcpfb_vec_) {
        size_t pos = rtcpfb.attr_string.find("nack");
        if (pos != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool SdpTransform::IsAudioNackEnable() {
    for (auto& rtcpfb : audio_rtcpfb_vec_) {
        size_t pos = rtcpfb.attr_string.find("nack");
        if (pos != std::string::npos) {
            return true;
        }
    }
    return false;

}
/*
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time 
 */
int SdpTransform::ParseExtMap(const std::string& line) {
    const std::string ext_attr("a=extmap");
    std::string ext_string = line.substr(ext_attr.length() + 1);

    size_t pos = ext_string.find(" ");
    if (pos == std::string::npos) {
        LogErrorf(logger_, "extmap error:%s", line.c_str());
        return -1;
    }

    int ext_id = atoi(ext_string.substr(0, pos).c_str());
    std::string desc = ext_string.substr(pos + 1);

    ExtInfo info = {
        .ext_id = ext_id,
        .desc   = desc
    };

    ext_map_[ext_id] = info;

    return 0;
}
/*
a=ssrc:339820557 msid:- 3d33599a-f48c-91d3-2325-74b7c98f9390
a=ssrc:339820557 cname:7rwvqwogyelwn97x
a=ssrc:3360589719 msid:- 3d33599a-f48c-91d3-2325-74b7c98f9390
a=ssrc:3360589719 cname:7rwvqwogyelwn97x
a=ssrc-group:FID 339820557 3360589719 
 */
int SdpTransform::ParseSsrcInfo(const std::string& line) {
    const std::string ssrc_attr("a=ssrc");
    const std::string cname_attr("cname");
    const std::string msid_attr("msid");

    std::string ssrc_string = line.substr(ssrc_attr.length() + 1);

    size_t pos = ssrc_string.find(" ");
    uint32_t ssrc = (uint32_t)atoi(ssrc_string.substr(0, pos).c_str());

    ssrc_string = ssrc_string.substr(pos + 1);

    pos = ssrc_string.find(cname_attr);
    if (pos == 0) {
        std::string cname = ssrc_string.substr(cname_attr.length() + 1);
        auto iter = ssrc_info_map_.find(ssrc);
        if (iter == ssrc_info_map_.end()) {
            SSRCInfo info;
            info.ssrc = ssrc;
            info.is_video = current_is_video_;
            info.cname = cname;
            ssrc_info_map_[ssrc] = info;
            return 0;
        }
        iter->second.ssrc = ssrc;
        iter->second.cname = cname;
        iter->second.is_video = current_is_video_;
        return 0;
    }

    pos = ssrc_string.find(msid_attr);
    if (pos == 0) {
        std::string msid_string = ssrc_string.substr(msid_attr.length() + 1);
        pos = msid_string.find(" ");
        if (pos == std::string::npos) {
            LogErrorf(logger_, "find msid in ssrc error:%s", msid_string.c_str());
            return -1;
        }
        std::string msid = msid_string.substr(0, pos);
        std::string msid_appdata = msid_string.substr(pos + 1);

        auto iter = ssrc_info_map_.find(ssrc);
        if (iter == ssrc_info_map_.end()) {
            SSRCInfo info;
            info.ssrc = ssrc;
            info.is_video = current_is_video_;
            info.msid = msid;
            info.msid_appdata = msid_appdata;
            ssrc_info_map_[ssrc] = info;
            return 0;
        }
        iter->second.ssrc = ssrc;
        iter->second.is_video = current_is_video_;
        iter->second.msid = msid;
        iter->second.msid_appdata = msid_appdata;
    }
    
    return 0;
}

int SdpTransform::ParseSsrcGroupFid(const std::string& line) {
    const std::string fid_attr = "a=ssrc-group:FID";
    std::string ssrcs_string = line.substr(fid_attr.length() + 1);
    std::vector<std::string> ssrc_vec;
    
    StringSplit(ssrcs_string, " ", ssrc_vec);

    if (ssrc_vec.size() < 2) {
        LogErrorf(logger_, "ssrc list size error:%lu", ssrc_vec.size());
        return -1;
    }

    if (current_is_video_) {
        for (auto& ssrc_str : ssrc_vec) {
            video_fid_vec_.push_back(atoi(ssrc_str.c_str()));
        }
    } else {
        for (auto& ssrc_str : ssrc_vec) {
            audio_fid_vec_.push_back(atoi(ssrc_str.c_str()));
        }
    }

    uint32_t main_ssrc = atoi(ssrc_vec[0].c_str());
    uint32_t slave_ssrc = atoi(ssrc_vec[1].c_str());

    auto main_iter = ssrc_info_map_.find(main_ssrc);
    if (main_iter == ssrc_info_map_.end()) {
        LogErrorf(logger_, "main ssrc find error:%u", main_ssrc);
        return -1;
    }
    main_iter->second.is_rtx   = false;
    main_iter->second.rtx_ssrc = slave_ssrc;

    auto slave_iter = ssrc_info_map_.find(slave_ssrc);
    if (slave_iter == ssrc_info_map_.end()) {
        LogErrorf(logger_, "slave ssrc find error:%u", slave_ssrc);
        return -1;
    }
    main_iter->second.is_rtx   = true;
    main_iter->second.rtx_ssrc = main_ssrc;

    return 0;
}

int SdpTransform::ParseLine(std::string line) {
    size_t pos = 0;
    const std::string candidate_attr("a=candidate:");
    const std::string ufrag_attr("a=ice-ufrag:");
    const std::string pwd_attr("a=ice-pwd:");
    const std::string ver_attr("v=");
    const std::string session_attr("o=");
    const std::string session_name_attr("s=");
    const std::string m_attr("m=");
    const std::string rtpmap_attr("a=rtpmap");
    const std::string fmtp_attr("a=fmtp");
    const std::string rtcp_fb_attr("a=rtcp-fb");
    const std::string ext_attr("a=extmap");
    const std::string ssrc_attr("a=ssrc");

    RemoveSubfix(line, "\r");

    //"v=0"
    pos = line.find(ver_attr);
    if (pos == 0) {
        proto_ver_ = line.substr(ver_attr.length());
        return 0;
    }

    //s=SRSPublishSession
    pos = line.find(session_name_attr);
    if (pos == 0) {
        pos = line.find("=");
        if (pos == std::string::npos) {
            LogErrorf(logger_, "session name attr error:%s", line.c_str());
            return 0;
        }
        session_name_ = line.substr(pos + 1);
        return 0;
    }

    //"o=SRS/6.0.48(Bee) 139736291876000 2 IN IP4 0.0.0.0"
    pos = line.find(session_attr);
    if (pos == 0) {
        std::vector<std::string> attr_vec;
        StringSplit(line, " ", attr_vec);
        if (attr_vec.size() < 2) {
            LogErrorf(logger_, "session attr error:%s", line.c_str());
            return 0;
        }
        session_id_ = attr_vec[0];
        pos = session_id_.find("=");
        if (pos == std::string::npos) {
            LogErrorf(logger_, "session attr error:%s", line.c_str());
            return 0;
        }
        session_id_ = session_id_.substr(pos + 1);
        return 0;
    }

    //m=audio 9 UDP/TLS/RTP/SAVPF 111
    //m=video 9 UDP/TLS/RTP/SAVPF 106 107
    pos = line.find(m_attr);
    if (pos == 0) {
        return ParseM(line);
    }

    //a=rtpmap:106 H264/90000
    pos = line.find(rtpmap_attr);
    if (pos == 0) {
        return ParseRtpMap(line);
    }

    //a=fmtp:106 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
    pos = line.find(fmtp_attr);
    if (pos == 0) {
        return ParseFmtp(line);
    }

    //a=rtcp-fb:106 goog-remb
    pos = line.find(rtcp_fb_attr);
    if (pos == 0) {
        return ParseRtcpFb(line);
    }

    //a=extmap:1 urn:ietf:params:rtp-hdrext:toffset
    pos = line.find(ext_attr);
    if (pos == 0) {
        return ParseExtMap(line);
    }
    //a=ssrc:1279799722 cname:ZDA1fh2h/FIKv0Lf
    //a=ssrc:1279799722 msid:7d3a1915-764a-43d3-be25-41a12def895a fc115548-c635-4565-85c7-84c3443ea453
    pos = line.find(ssrc_attr);
    if (pos == 0) {
        return ParseSsrcInfo(ssrc_attr);
    }

    pos = line.find(candidate_attr);
    if (pos == 0) {
        IceInfo ice_info;
        std::string candidate = line.substr(candidate_attr.length());
        std::vector<std::string> candidate_items;

        StringSplit(candidate, " ", candidate_items);
        if (candidate_items.size() > 6) {
            if (candidate_items[2] == "tcp") {
                ice_info.net_type = ICE_TCP;
            } else if (candidate_items[2] == "udp") {
                ice_info.net_type = ICE_UDP;
            } else {
                ice_info.net_type = ICE_NET_UNKNOWN;
            }
            ice_info.hostip = candidate_items[4];
            ice_info.port   = atoi(candidate_items[5].c_str());

            bool repeat = false;
            for (auto& info : dtls_->ice_infos) {
                if (info == ice_info) {
                    repeat = true;
                    break;
                }
            }
            if (!repeat) {
                LogInfof(logger_, "ice net type:%s, hostip:%s, port:%d",
                        candidate_items[2].c_str(), ice_info.hostip.c_str(), ice_info.port);
                dtls_->ice_infos.push_back(ice_info);
            }
        }
        return 0;
    }

    pos = line.find(ufrag_attr);
    if (pos == 0) {
        if (!dtls_->remote_fragment_.empty()) {
            return 0;
        }
        dtls_->remote_fragment_ = line.substr(ufrag_attr.length());
        LogInfof(logger_, "remote ice ufrag:%s", dtls_->remote_fragment_.c_str());
        return 0;
    }

    pos = line.find(pwd_attr);
    if (pos == 0) {
        if (!dtls_->remote_pwd_.empty()) {
            return 0;
        }
        dtls_->remote_pwd_ = line.substr(pwd_attr.length());
        LogInfof(logger_, "remote ice pwd:%s", dtls_->remote_pwd_.c_str());
        return 0;
    }

    return 0;
}

std::string SdpTransform::GetProtoVersion() {
    std::stringstream ss;
    ss << "v=" << proto_ver_ << "\n";
    return ss.str();
}

std::string SdpTransform::GetSessionIdentifier() {
    std::stringstream ss;
    session_id_ = UUID::MakeNumString(19);
    ss << "o=- " << session_id_ << " 2";
    ss << " IN IP4 127.0.0.1\n";
    return ss.str();
}


std::string SdpTransform::GetSessionName() {
    std::stringstream ss;
    if (session_name_.empty()) {
        session_name_ = "cpp_streamer";
    }
    ss << "s=" << session_name_ << "\n";
    return ss.str();
}

std::string SdpTransform::GetTimeScale(int64_t start, int64_t end) {
    std::stringstream ss;
    ss << "t=" << start << " " << end << "\n";
    return ss.str();
}

std::string SdpTransform::GetBasicAttr(bool has_video, bool has_audio) {
    std::stringstream ss;
    int index = 0;

    ss << "a=extmap-allow-mixed\n";
    ss << "a=msid-semantic: WMS\n";
    ss << "a=group:BUNDLE";
    if (has_video) {
        ss << " " << index++;
    }
    if (has_audio) {
        ss << " " << index++;
    }
    ss << "\n";
    return ss.str();
}

std::string SdpTransform::GetConnection() {
    return "c=IN IP4 0.0.0.0\n";
}

std::string SdpTransform::GetMediaDesc(bool is_video) {
    std::stringstream ss;

    ss << "m=";
    ss << (is_video ? "video" : "audio");
    ss << " 9 ";
    ss << "UDP/TLS/RTP/SAVPF";

    if (is_video) {
        for (int payload : video_pt_vec_) {
            ss << " ";
            ss << payload;
        }
    } else {
        for (int payload : audio_pt_vec_) {
            ss << " ";
            ss << payload;
        }
    }
    ss << "\n";
  
    return ss.str();
}

/*
a=rtpmap:106 H264/90000
a=rtpmap:107 rtx/90000
a=rtpmap:111 opus/48000/2
*/
std::string SdpTransform::GetRtpMap(bool is_video) {
    std::stringstream ss;

    if (is_video) {
        for (auto& item : video_rtp_map_infos_) {
            ss << "a=rtpmap:";
            ss << item.second.payload_type;
            ss << " ";
            ss << item.second.codec_type;
            ss << "/";
            ss << item.second.clock_rate;
            ss << "\n";
        }
    } else {
        for (auto& item : audio_rtp_map_infos_) {
            ss << "a=rtpmap:";
            ss << item.second.payload_type;
            ss << " ";
            ss << item.second.codec_type;
            ss << "/";
            ss << item.second.clock_rate;
            if (item.second.channel > 0) {
                ss << "/";
                ss << item.second.channel;
            }
            ss << "\n";
        }
    }
    return ss.str();
}

//a=fmtp:106 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
//a=fmtp:107 apt=106
//a=fmtp:111 minptime=10;useinbandfec=1
std::string SdpTransform::GetFmtp(bool is_video) {
    std::stringstream ss;

    if (is_video) {
        for (auto& item : video_fmtp_vec_) {
            ss << "a=fmtp:" << item.payload_type;
            ss << " " << item.attr_string;
            ss << "\n";
        }
        /*
        ss << "a=fmtp:" << vpayload_type_ << " ";
        ss << "x-google-start-bitrate=" << start_kbitrate_ << "\n";

        ss << "a=fmtp:" << vpayload_type_ << " ";
        ss << "x-google-min-bitrate=" << min_kbitrate_ << "\n";


        ss << "a=fmtp:" << vpayload_type_ << " ";
        ss << "x-google-max-bitrate=" << max_kbitrate_ << "\n";
        */
    } else {
        for (auto& item : audio_fmtp_vec_) {
            ss << "a=fmtp:" << item.payload_type;
            ss << " " << item.attr_string;
            ss << "\n";
        }
    }
    return ss.str();
}

std::string SdpTransform::GetRtcp(bool is_video) {
    std::stringstream ss;

    ss << "a=rtcp:9 IN IP4 0.0.0.0\n";

    //rtcpfb_vec_
    if (is_video) {
        for (auto& item : video_rtcpfb_vec_) {
            ss << "a=rtcp-fb:" << item.payload_type << " " << item.attr_string << "\n";
        }
    } else {
        for (auto& item : audio_rtcpfb_vec_) {
            ss << "a=rtcp-fb:" << item.payload_type << " " << item.attr_string << "\n";
        }
    }

    return ss.str();
}

std::string SdpTransform::GetExtMap(bool is_video) {
    std::stringstream ss;

    if (is_video) {
        if (em_toffset_ > 0) {
            ss << "a=extmap:" << em_toffset_ << " urn:ietf:params:rtp-hdrext:toffset\n";
        }
        if (em_abs_send_time_ > 0) {
            ss << "a=extmap:" << em_abs_send_time_ << " http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\n";
        }
        if (em_video_rotation_ > 0) {
            ss << "a=extmap:" << em_video_rotation_ << " urn:3gpp:video-orientation\n";
        }
        if (em_tcc_ > 0) {
            ss << "a=extmap:" << em_tcc_ << " http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\n";
        }
        if (em_playout_delay_ > 0) {
            ss << "a=extmap:" << em_playout_delay_ << " http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\n";
        }
        if (em_video_content_ > 0) {
            ss << "a=extmap:" << em_video_content_ << " http://www.webrtc.org/experiments/rtp-hdrext/video-content-type\n";
        }
        if (em_video_timing_ > 0) {
            ss << "a=extmap:" << em_video_timing_ << " http://www.webrtc.org/experiments/rtp-hdrext/video-timing\n";
        }
        if (em_color_space_ > 0) {
            ss << "a=extmap:" << em_color_space_ << " http://www.webrtc.org/experiments/rtp-hdrext/color-space\n";
        }
        if (em_sdes_ > 0) {
            ss << "a=extmap:" << em_sdes_ << " urn:ietf:params:rtp-hdrext:sdes:mid\n";
        }
        if (em_rtp_streamid_ > 0) {
            ss << "a=extmap:" << em_rtp_streamid_ << " urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id\n";
        }
        if (em_rp_rtp_streamid_ > 0) {
            ss << "a=extmap:" << em_rp_rtp_streamid_ << " urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id\n";
        }
    } else {
        if (em_audio_level_ > 0) {
            ss << "a=extmap:" << em_audio_level_ << " urn:ietf:params:rtp-hdrext:ssrc-audio-level\n";
        }
        if (em_abs_send_time_ > 0) {
            ss << "a=extmap:" << em_abs_send_time_ << " http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\n";
        }
        if (em_tcc_ > 0) {
            ss << "a=extmap:" << em_tcc_ << " http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\n";
        }
        if (em_sdes_ > 0) {
            ss << "a=extmap:" << em_sdes_ << " urn:ietf:params:rtp-hdrext:sdes:mid\n";
        }
    }
    return ss.str();
}

std::string SdpTransform::GetMidInfo(bool is_video) {
    std::stringstream ss;

    if (is_video) {
        v_msid_ = v_msid_.empty() ? UUID::MakeUUID() : v_msid_;
        v_msid_appdata_ = v_msid_appdata_.empty() ? UUID::MakeUUID2() : v_msid_appdata_;
    } else {
        a_msid_ = a_msid_.empty() ? UUID::MakeUUID() : a_msid_;
        a_msid_appdata_ = a_msid_appdata_.empty() ? UUID::MakeUUID2() : a_msid_appdata_;
    }
    ss << "a=setup:" << setup_ << "\n";
    if (is_video) {
        if (vmid_ >= 0) {
            ss << "a=mid:" << vmid_;
            ss << "\n";
        }
        if (!v_msid_.empty() && !v_msid_appdata_.empty()) {
            ss << "a=msid:" << v_msid_;
            ss << " " << v_msid_appdata_;
            ss << "\n";
        }
    } else {
        if (amid_) {
            ss << "a=mid:" << amid_;
            ss << "\n";
        }

        if (!a_msid_.empty() && !a_msid_appdata_.empty()) {
            ss << "a=msid:" << a_msid_;
            ss << " " << a_msid_appdata_;
            ss << "\n";
        }
    }
    ss << "a=" << direction_ << "\n";
    
    return ss.str();
}

std::string SdpTransform::GetIceInfo(RtcDtls* dtls) {
    std::stringstream ss;

    ss << "a=ice-ufrag:" << dtls->local_fragment_ << "\n";
    ss << "a=ice-pwd:" << dtls->local_pwd_ << "\n";
    ss << "a=fingerprint:" << dtls->fg_algorithm_ << " " << dtls->fingerprint_ << "\n";
    ss << "a=ice-options:trickle\n";

    return ss.str();
}
/*
a=ssrc:4255034931 cname:tj5+r5YWeiHIF+5n
a=ssrc:4255034931 msid:- d21a6392-a5f0-450d-ac79-d4494eca0142
a=ssrc:274918202 cname:tj5+r5YWeiHIF+5n
a=ssrc:274918202 msid:- d21a6392-a5f0-450d-ac79-d4494eca0142
a=ssrc-group:FID 4255034931 274918202
a=rtcp-mux
a=rtcp-rsize 
 */
std::string SdpTransform::GetSsrcs(bool is_video) {
    std::stringstream ss;

    if (is_video) {
        std::stringstream ss_video;
        std::stringstream ss_rtx;
        for(auto& ssrc_item : ssrc_info_map_) {
            if (!ssrc_item.second.is_video) {
                continue;
            }
            if (ssrc_item.second.is_rtx) {
                ss_rtx << "a=ssrc:" << ssrc_item.second.ssrc;
                ss_rtx << " msid:" << ssrc_item.second.msid;
                ss_rtx << " " << ssrc_item.second.msid_appdata;
                ss_rtx << "\n";
                ss_rtx << "a=ssrc:" << ssrc_item.second.ssrc;
                ss_rtx << " cname:" << ssrc_item.second.cname;
                ss_rtx << "\n";

                video_rtx_ssrc_ = ssrc_item.second.ssrc;
            } else {
                ss_video << "a=ssrc:" << ssrc_item.second.ssrc;
                ss_video << " msid:" << ssrc_item.second.msid;
                ss_video << " " << ssrc_item.second.msid_appdata;
                ss_video << "\n";
                ss_video << "a=ssrc:" << ssrc_item.second.ssrc;
                ss_video << " cname:" << ssrc_item.second.cname;
                ss_video << "\n";

                video_ssrc_ = ssrc_item.second.ssrc;
            }
        }
        ss << ss_video.str() << ss_rtx.str();

        ss << "a=ssrc-group:FID " << video_ssrc_ << " " << video_rtx_ssrc_ << "\n";
        ss << "a=rtcp-mux\n";
        ss << "a=rtcp-rsize\n";
    } else {
        for(auto& ssrc_item : ssrc_info_map_) {
            if (ssrc_item.second.is_video) {
                continue;
            }
            ss << "a=ssrc:" << ssrc_item.second.ssrc;
            ss << " msid:" << ssrc_item.second.msid;
            ss << " " << ssrc_item.second.msid_appdata;
            ss << "\n";

            ss << "a=ssrc:" << ssrc_item.second.ssrc;
            ss << " cname:" << ssrc_item.second.cname;
            ss << "\n";
            audio_ssrc_ = ssrc_item.second.ssrc;
        }

        audio_ssrc_ = (audio_ssrc_ == 0) ? ByteCrypto::GetRandomUint(0, 0xffffffff) : audio_ssrc_;
        audio_cname_ = ByteCrypto::GetRandomString(16);
        ss << "a=ssrc:" << audio_ssrc_ << " msid:- " << a_msid_ << "\n";
        ss << "a=ssrc:" << audio_ssrc_ << " cname:" << audio_cname_ << "\n";
        ss << "a=rtcp-mux\n";
    }
    return ss.str();
}

std::string SdpTransform::GetVideoDesc() {
    std::stringstream ss;

    ss << GetMediaDesc(true);
    ss << GetConnection();
    ss << GetRtpMap(true);
    ss << GetFmtp(true);
    ss << GetRtcp(true);
    ss << GetExtMap(true);
    ss << GetMidInfo(true);
    ss << GetIceInfo(dtls_);
    ss << GetSsrcs(true);

    return ss.str();
}

std::string SdpTransform::GetAudioDesc() {
    std::stringstream ss;

    ss << GetMediaDesc(false);
    ss << GetConnection();
    ss << GetRtpMap(false);
    ss << GetFmtp(false);
    ss << GetRtcp(false);
    ss << GetExtMap(false);
    ss << GetMidInfo(false);
    ss << GetIceInfo(dtls_);
    ss << GetSsrcs(false);

    return ss.str();
}

int SdpTransform::GetVideoPayloadType() {
    for (auto& item : video_fmtp_vec_) {
        if (item.is_video && !item.is_rtx) {
            return item.payload_type;
        }
    }
    return -1;
}

int SdpTransform::GetVideoRtxPayloadType() {
    for (auto& item : video_fmtp_vec_) {
        if (item.is_video && item.is_rtx) {
            return item.payload_type;
        }
    }
    return -1;
}

int SdpTransform::GetAudioPayloadType() {
    LogInfof(logger_, "audio fmtp vec size:%lu", audio_fmtp_vec_.size());
    for (auto& item : audio_fmtp_vec_) {
        LogInfof(logger_, "audio fmtp item payload_type:%d, is video:%d", item.payload_type, item.is_video);
        if (!item.is_video) {
            return item.payload_type;
        }
    }
    return -1;
}

void SdpTransform::SetVideoRtxFlag(bool flag) { 
    rtx_enable_ = flag;
}

bool SdpTransform::IsVideoRtxEnable() {
     for (auto& item : video_fmtp_vec_) {
        if (item.is_video && item.is_rtx) {
            rtx_enable_ = true;
            return true;
        }
    }
    rtx_enable_ = false;
    return false;
   
}

int SdpTransform::GetVideoClockRate() {
    return video_clock_rate_;
}

int SdpTransform::GetVideoRtxClockRate() {
    return video_rtx_clock_rate_;
}

int SdpTransform::GetAudioClockRate() {
    return audio_clock_rate_;
}

}

