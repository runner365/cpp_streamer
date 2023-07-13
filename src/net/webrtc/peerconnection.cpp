#include "peerconnection.hpp"
#include "stun.hpp"
#include "rtprtcp_pub.hpp"
#include "rtcp_fb_pub.hpp"
#include "rtcp_rr.hpp"
#include "rtcp_sr.hpp"
#include "rtcp_xr_dlrr.hpp"
#include "rtcp_xr_rrt.hpp"
#include "rtcpfb_nack.hpp"
#include "srtp_session.hpp"
#include "uuid.hpp"
#include "byte_crypto.hpp"
#include <cstring>
#include <sstream>

namespace cpp_streamer
{
PeerConnection::PeerConnection(uv_loop_t* loop, 
        Logger* logger, PCStateReportI* state_report):TimerInterface(loop, 300)
                                                      , loop_(loop)
                                                      , logger_(logger)
                                                      , state_report_(state_report)
                                                      , dtls_(this, logger)
                                                      , offer_sdp_(&dtls_, logger)
                                                      , answer_sdp_(&dtls_, logger)
                                                      , jb_video_(MEDIA_VIDEO_TYPE, this, loop, logger)
                                                      , jb_audio_(MEDIA_AUDIO_TYPE, this, loop, logger)
{
    udp_client_ = new UdpClient(loop, this, logger, nullptr, 0);
    dtls_.udp_client_ = udp_client_;

    memset(&last_xr_ntp_, 0, sizeof(last_xr_ntp_));

    SRtpSession::Init(logger);
}

PeerConnection::~PeerConnection()
{
    LogInfof(logger_, "destruct PeerConnection");
    StopTimer();
    if (udp_client_) {
        delete udp_client_;
        udp_client_ = nullptr;
    }
    if (write_srtp_) {
        delete write_srtp_;
        write_srtp_ = nullptr;
    }
    if (read_srtp_) {
        delete read_srtp_;
        read_srtp_ = nullptr;
    }
    if (video_send_stream_) {
        delete video_send_stream_;
        video_send_stream_ = nullptr;
    }

    if (audio_send_stream_) {
        delete audio_send_stream_;
        audio_send_stream_ = nullptr;
    }
}

std::string PeerConnection::GetDirectionString(WebRtcSdpDirection direction_type) {
    switch (direction_type)
    {
    case SEND_ONLY:
        return "sendonly";
    case RECV_ONLY:
        return "recvonly";
    case SEND_RECV:
        return "sendrecv";
    default:
        return "";
    }
    return "";
}

int PeerConnection::ParseAnswerSdp(const std::string& sdp) {
    int ret = answer_sdp_.Parse(sdp);

    if (ret < 0) {
        LogErrorf(logger_, "answer sdp parse error");
        return ret;
    }

    if (answer_sdp_.ssrc_info_map_.empty()) {
        answer_sdp_.ssrc_info_map_ = offer_sdp_.ssrc_info_map_;
    }
    LogInfof(logger_, "video ssrc:%u, video rtx ssrc:%u, audio ssrc:%u, rtx_enable:%s",
                answer_sdp_.GetVideoSsrc(),
                answer_sdp_.GetVideoRtxSsrc(),
                answer_sdp_.GetAudioSsrc(),
                answer_sdp_.IsVideoRtxEnable() ? "enable" : "disable");
    LogInfof(logger_, "video pt:%d, video rtx pt:%d, audio pt:%d, video rate:%d, video rtx rate:%d, audio rate:%d",
            answer_sdp_.GetVideoPayloadType(),
            answer_sdp_.GetVideoRtxPayloadType(),
            answer_sdp_.GetAudioPayloadType(),
            answer_sdp_.GetVideoClockRate(),
            answer_sdp_.GetVideoRtxClockRate(),
            answer_sdp_.GetAudioClockRate());
 
    LogInfof(logger_, "video nack:%s, audio nack:%s",
            answer_sdp_.IsVideoNackEnable() ? "true" : "false",
            answer_sdp_.IsAudioNackEnable() ? "true" : "false");
    if (pc_state_ >= PC_SDP_DONE_STATE) {
        LogWarnf(logger_, "ParseAnswerSdp error:  peer connection state is %d", pc_state_);
        return 0;
    }
    pc_state_ = PC_SDP_DONE_STATE;
    //LogInfof(logger_, "answer sdp:%s", answer_sdp_.GenSdpString().c_str());
    Report("sdp", "ready");

    StartTimer();
    return ret;
}


void PeerConnection::SendStun(int64_t now_ms) {
    if (pc_state_ < PC_SDP_DONE_STATE) {
        return;
    }

    if (last_stun_ms_ <= 0) {
        last_stun_ms_ = now_ms;
    } else {
        if (now_ms - last_stun_ms_ < 800) {
            return;
        }
        last_stun_ms_ = now_ms;
    }
    StunPacket pkt;

    if (dtls_.ice_infos.empty()) {
        LogErrorf(logger_, "dtls ice information is empty");
        return;
    }


    IceInfo& ice = dtls_.ice_infos[0];
    assert(ice.net_type == ICE_UDP);

    UdpTuple remote_address(ice.hostip, ice.port);
    dtls_.remote_address_ = remote_address;
    
    pkt.username_ = dtls_.remote_fragment_;
    pkt.username_ += ":";
    pkt.username_ += dtls_.local_fragment_;
    LogDebugf(logger_, "stun username:%s", pkt.username_.c_str());
    LogDebugf(logger_, "dtls remote frag:%s", dtls_.remote_fragment_.c_str());
    LogDebugf(logger_, "dtls local frag:%s", dtls_.local_fragment_.c_str());
    pkt.password_ = dtls_.remote_pwd_;
    pkt.has_use_candidate_ = true;
    pkt.add_msg_integrity_ = true;
    pkt.priority_ = 100;

    pkt.Serialize();

    udp_client_->Write((char*)pkt.data_, pkt.data_len_, remote_address);
    udp_client_->TryRead();
}

/*
need to initialize:
1) username_: 
snprintf(username, sizeof(username), "%s:%s",
      dtls->remote_fragment_, rtc->local_fragment_);
2) add_msg_integrity_, dtls->remote_pwd_ for password_;
ByteCrypto::GetHmacSha1(password_,...
*/
std::string PeerConnection::CreateOfferSdp(WebRtcSdpDirection direction_type) {
    direct_type_ = direction_type;
    offer_sdp_.direction_ = GetDirectionString(direction_type);
    offer_sdp_.SetVideoRtxFlag(true);

    LogInfof(logger_, "peer connection create offer sdp, direct:%d",
            direction_type);

    //start:mid, msid
    offer_sdp_.vmid_ = 0;
    offer_sdp_.amid_ = 1;

    offer_sdp_.v_msid_ = UUID::MakeUUID();
    offer_sdp_.a_msid_ = UUID::MakeUUID();

    offer_sdp_.v_msid_appdata_ = UUID::MakeUUID2();
    offer_sdp_.a_msid_appdata_ = UUID::MakeUUID2();
    //end:mid, msid
    
    //video audio payload list
    offer_sdp_.video_pt_vec_.push_back(VPLAYLOAD_DEF_TYPE);
    offer_sdp_.video_pt_vec_.push_back(RTX_PAYLOAD_DEF_TYPE);

    offer_sdp_.audio_pt_vec_.push_back(APLAYLOAD_DEF_TYPE);

    //start: audio/video RtpMap
    RtpMapInfo h264RtpInfo = {
        .payload_type = VPLAYLOAD_DEF_TYPE,
        .codec_type   = "H264",
        .clock_rate   = 90000
    };
    offer_sdp_.video_rtp_map_infos_[VPLAYLOAD_DEF_TYPE] = h264RtpInfo;

    RtpMapInfo rtxRtpInfo = {
        .payload_type = RTX_PAYLOAD_DEF_TYPE,
        .codec_type   = "rtx",
        .clock_rate   = 90000
    };
    offer_sdp_.video_rtp_map_infos_[RTX_PAYLOAD_DEF_TYPE] = rtxRtpInfo;

    RtpMapInfo opusRtpInfo = {
        .payload_type = APLAYLOAD_DEF_TYPE,
        .codec_type   = "opus",
        .clock_rate   = 48000,
        .channel      = 2
    };
    offer_sdp_.audio_rtp_map_infos_[APLAYLOAD_DEF_TYPE] = opusRtpInfo;
    //end: audio/video RtpMap
    
    //start: audio/video fmtp info
    FmtpInfo h264FmtpInfo = {
        .payload_type = VPLAYLOAD_DEF_TYPE,
        .attr_string  = "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f",
        .is_video     = true,
        .is_rtx       = false,
        .rtx_payload_type = RTX_PAYLOAD_DEF_TYPE
    
    };
    offer_sdp_.video_fmtp_vec_.push_back(h264FmtpInfo);

    std::string rtxFmtpStr = "apt=";
    rtxFmtpStr += std::to_string(VPLAYLOAD_DEF_TYPE);
    FmtpInfo rtxFmtpInfo = {
        .payload_type = RTX_PAYLOAD_DEF_TYPE,
        .attr_string  = rtxFmtpStr,
        .is_video     = true,
        .is_rtx       = true,
        .rtx_payload_type = 0
    
    };
    offer_sdp_.video_fmtp_vec_.push_back(rtxFmtpInfo);

    FmtpInfo opusFmtpInfo = {
        .payload_type = APLAYLOAD_DEF_TYPE,
        .attr_string  = "minptime=10;useinbandfec=1",
        .is_video     = false,
        .is_rtx       = false,
        .rtx_payload_type = 0
    };
    offer_sdp_.audio_fmtp_vec_.push_back(opusFmtpInfo);
    //end: audio/video fmtp info
    
    //start: rtcpFb
    RtcpFbInfo remb = {
        .payload_type = VPLAYLOAD_DEF_TYPE,
        .attr_string  = "goog-remb"
    };
    offer_sdp_.video_rtcpfb_vec_.push_back(remb);

    RtcpFbInfo transport_cc = {
        .payload_type = VPLAYLOAD_DEF_TYPE,
        .attr_string  = "transport-cc"
    };
    offer_sdp_.video_rtcpfb_vec_.push_back(transport_cc);

    transport_cc = {
        .payload_type = APLAYLOAD_DEF_TYPE,
        .attr_string  = "transport-cc"
    };
    offer_sdp_.audio_rtcpfb_vec_.push_back(transport_cc);

    RtcpFbInfo fir = {
        .payload_type = VPLAYLOAD_DEF_TYPE,
        .attr_string  = "cmm fir"
    };
    offer_sdp_.video_rtcpfb_vec_.push_back(fir);

    RtcpFbInfo nack_fb {
        .payload_type = VPLAYLOAD_DEF_TYPE,
        .attr_string  = "nack"
    };
    offer_sdp_.video_rtcpfb_vec_.push_back(nack_fb);

    RtcpFbInfo nack_pli {
        .payload_type = VPLAYLOAD_DEF_TYPE,
        .attr_string  = "nack pli"
    };
    offer_sdp_.video_rtcpfb_vec_.push_back(nack_pli);
    //end: rtcpFb
    
    //Start: ssrc info
    offer_sdp_.video_ssrc_ = ByteCrypto::GetRandomUint(1, 0xffffffff);
    offer_sdp_.video_rtx_ssrc_ = ByteCrypto::GetRandomUint(1, 0xffffffff);
    offer_sdp_.audio_ssrc_ = ByteCrypto::GetRandomUint(1, 0xffffffff);

    offer_sdp_.video_cname_ = ByteCrypto::GetRandomString(16);
    offer_sdp_.audio_cname_ = ByteCrypto::GetRandomString(16);

    LogInfof(logger_, "video ssrc:%u, rtx ssrc:%u",
            offer_sdp_.video_ssrc_,
            offer_sdp_.video_rtx_ssrc_);
    SSRCInfo video_ssrc_info = {
        .ssrc = offer_sdp_.video_ssrc_,
        .is_video = true,
        .is_rtx   = false,
        .msid     = offer_sdp_.v_msid_,
        .msid_appdata = offer_sdp_.v_msid_appdata_,
        .cname = offer_sdp_.video_cname_,
        .rtx_ssrc = offer_sdp_.video_rtx_ssrc_
    };
    offer_sdp_.ssrc_info_map_[offer_sdp_.video_ssrc_] = video_ssrc_info;

    SSRCInfo rtx_ssrc_info = {
        .ssrc = offer_sdp_.video_rtx_ssrc_,
        .is_video = true,
        .is_rtx   = true,
        .msid     = offer_sdp_.v_msid_,
        .msid_appdata = offer_sdp_.v_msid_appdata_,
        .cname = offer_sdp_.video_cname_,
        .rtx_ssrc = offer_sdp_.video_ssrc_

    };
    offer_sdp_.ssrc_info_map_[offer_sdp_.video_rtx_ssrc_] = rtx_ssrc_info;

    SSRCInfo audio_ssrc_info = {
        .ssrc = offer_sdp_.audio_ssrc_,
        .is_video = false,
        .is_rtx   = false,
        .msid     = offer_sdp_.a_msid_,
        .msid_appdata = offer_sdp_.a_msid_appdata_,
        .cname = offer_sdp_.audio_cname_,
        .rtx_ssrc = 0
    };
    offer_sdp_.ssrc_info_map_[offer_sdp_.audio_ssrc_] = audio_ssrc_info;
    //end: ssrc info
    
    //start: ExtMap
    offer_sdp_.em_toffset_         = 1;
    offer_sdp_.em_abs_send_time_   = 2;
    offer_sdp_.em_video_rotation_  = 3;
    offer_sdp_.em_tcc_             = 4;
    offer_sdp_.em_playout_delay_   = 5;
    offer_sdp_.em_video_content_   = 6;
    offer_sdp_.em_video_timing_    = 7;
    offer_sdp_.em_color_space_     = 8;
    offer_sdp_.em_sdes_            = 9;
    offer_sdp_.em_rtp_streamid_    = 10;
    offer_sdp_.em_rp_rtp_streamid_ = 11;

    offer_sdp_.em_audio_level_     = 14;
    //end: ExtMap
    
    if (dtls_.SslContextInit() < 0) {
        LogErrorf(logger_, "SslContextInit error");
        return "";
    }
    return offer_sdp_.GenSdpString(true, true);
}

int PeerConnection::HandleRtcpSr(uint8_t* data, int len) {
    int ret = sizeof(RtcpCommonHeader) + sizeof(RtcpSrHeader);

    return ret;
}

int PeerConnection::HandleRtcpRr(uint8_t* data, int len) {
    int ret = sizeof(RtcpCommonHeader) + sizeof(RtcpRrBlock);
    if (len <= ret) {
        return len;
    }

    RtcpRrPacket* rr_pkt = RtcpRrPacket::Parse(data, len);

    //LogDebugf(logger_, "rtcp rr dump:%s", rr_pkt->Dump().c_str());
    try {
        for (RtcpRrBlockInfo& block_info : rr_pkt->GetRrBlocks()) {
            uint32_t ssrc = block_info.GetReporteeSsrc();

            if (video_send_stream_ && ssrc == video_send_stream_->GetSsrc()) {
                video_send_stream_->HandleRtcpRr(block_info);
            }

            if (audio_send_stream_ && ssrc == audio_send_stream_->GetSsrc()) {
                audio_send_stream_->HandleRtcpRr(block_info);
            }
 
        }

    } catch (CppStreamException& e) {
        LogErrorf(logger_, "handle rtcp rr exception:%s", e.what());
    }

    delete rr_pkt;
    return len;
}

int PeerConnection::HandleRtcpRtpFb(uint8_t* data, int data_len) {
    if (data_len <= (int)sizeof(RtcpFbCommonHeader)) {
        return data_len;
    }

    RtcpFbCommonHeader* header = (RtcpFbCommonHeader*)data;
    switch (header->fmt)
    {
        case FB_RTP_NACK:
        {
            RtcpFbNack* nack_pkt = nullptr;
            try {
                nack_pkt = RtcpFbNack::Parse(data, data_len);
                uint32_t ssrc = nack_pkt->GetMediaSsrc();

                if (video_send_stream_ && ssrc == video_send_stream_->GetSsrc()) {
                    video_send_stream_->HandleRtcpNack(nack_pkt);
                }

                if (audio_send_stream_ && ssrc == audio_send_stream_->GetSsrc()) {
                    audio_send_stream_->HandleRtcpNack(nack_pkt);
                }
                delete nack_pkt;
            } catch(CppStreamException& e) {
                LogErrorf(logger_, "rtcp feedback nack error:%s", e.what());
                return -1;
            }
            break;
        }
        default:
        {
            LogWarnf(logger_, "receive rtcp psfb format(%d) is not handled.", header->fmt);
            break;
        }
    }
    return data_len;
}

int PeerConnection::HandleRtcpPsFb(uint8_t* data, int data_len) {

    return data_len;
}


int PeerConnection::HandleXrDlrr(XrDlrrData* dlrr_block) {
    uint32_t ssrc = ntohl(dlrr_block->ssrc);

    LogInfof(logger_, "Handle Xr Dlrr ssrc:%u", ssrc);
    if (video_send_stream_ && ssrc == video_send_stream_->GetSsrc()) {
        video_send_stream_->HandleXrDlrr(dlrr_block);
    }

    if (audio_send_stream_ && ssrc == audio_send_stream_->GetSsrc()) {
        audio_send_stream_->HandleXrDlrr(dlrr_block);
    }
    return 0;
}

int PeerConnection::HandleRtcpXr(uint8_t* data, int data_len) {
    RtcpCommonHeader* header = (RtcpCommonHeader*)data;
    uint32_t* ssrc_p         = (uint32_t*)(header + 1);
    RtcpXrHeader* xr_hdr     = (RtcpXrHeader*)(ssrc_p + 1);
    int64_t xr_len           = data_len - sizeof(RtcpCommonHeader) - 4;

    while(xr_len > 0) {
        switch(xr_hdr->bt)
        {
            case XR_DLRR:
            {
                XrDlrrData* dlrr_block = (XrDlrrData*)xr_hdr;
                HandleXrDlrr(dlrr_block);
                break;
            }
            case XR_RRT:
            {
                XrRrtData* rrt_block = (XrRrtData*)xr_hdr;
                last_xr_ntp_.ntp_sec  = ntohl(rrt_block->ntp_sec);
                last_xr_ntp_.ntp_frac = ntohl(rrt_block->ntp_frac);
                last_xr_ms_ = now_millisec();
                break;
            }
            default:
            {
                LogErrorf(logger_, "handle unkown xr type:%d", xr_hdr->bt);
            }
        }
        int64_t offset = 4 + ntohs(xr_hdr->block_length)*4;
        xr_len -= offset;
        data   += offset;
        xr_hdr = (RtcpXrHeader*)data;
    }
    return data_len;
}

void PeerConnection::HandleRtcp(uint8_t* data, size_t len) {
    if (!read_srtp_) {
        return;
    }
    if (read_srtp_->DecryptSrtcp(data, &len) == false) {
        return;
    }
    //handle rtcp packet
    int left_len = (int)len;
    uint8_t* p = data;

    while (left_len > 0) {
        RtcpCommonHeader* header = (RtcpCommonHeader*)p;
        uint16_t payload_length = GetRtcpLength(header);
        int item_total = (int)sizeof(RtcpCommonHeader) + payload_length;
        int ret = 0;

        LogDebugf(logger_, "rtcp type:%d", header->packet_type);
        switch (header->packet_type)
        {
            case RTCP_SR:
                {
                    ret = HandleRtcpSr(p, item_total);
                    break;
                }
            case RTCP_RR:
                {
                    ret = HandleRtcpRr(p, item_total);
                    break;
                }
            case RTCP_SDES:
                {
                    //rtcp sdes packet needn't to be handled.
                    break;
                }
            case RTCP_BYE:
                {
                    break;
                }
            case RTCP_APP:
                {
                    break;
                }
            case RTCP_RTPFB:
                {
                    ret = HandleRtcpRtpFb(p, item_total);
                    break;
                }
            case RTCP_PSFB:
                {
                    ret = HandleRtcpPsFb(p, item_total);
                    break;
                }
            case RTCP_XR:
                {
                    ret = HandleRtcpXr(p, item_total);
                    break;
                }
            default:
                {
                    LogErrorf(logger_, "unkown rtcp type:%d", header->packet_type);
                    ret = left_len;
                }
        }
        p        += ret;
        left_len -= ret;
    }

    return;
}

void PeerConnection::OnWrite(size_t sent_size, UdpTuple address) {
    return;
}

void PeerConnection::OnRead(const char* data, size_t data_size, UdpTuple address) {
    if (StunPacket::IsStun((uint8_t*)data, data_size)) {
        try {
            StunPacket* pkt = StunPacket::Parse((uint8_t*)data, data_size);
            if (pkt) {
                uint16_t port = 0;
                std::string ip = GetIpStr(pkt->xor_address_, port);
                if ((pc_udp_port_ == 0) || pc_ipaddr_str_.empty()
                    || (ip != pc_ipaddr_str_) || (port != pc_udp_port_)) {
                    LogInfof(logger_, "stun response update pc ipaddr:%s:%d",
                            ip.c_str(), port);
                    pc_ipaddr_str_ = ip;
                    pc_udp_port_ = port;
                }
                delete pkt;
            }
            //LogInfof(logger_, "receive stun packet:%s", pkt->Dump().c_str());
            if (pc_state_ < PC_STUN_DONE_STATE) {
                pc_state_ = PC_STUN_DONE_STATE;
                Report("stun", "ready");
                dtls_.DtlsStart();
            }
        } catch(CppStreamException& e) {
            LogErrorf(logger_, "handle stun packet exception:%s", e.what());
            return;
        }
    } else if (IsRtcp((uint8_t*)data, data_size)) {
        HandleRtcp((uint8_t*)data, data_size);
    } else if (IsRtp((uint8_t*)data, data_size)) {
        HandleRtpData((uint8_t*)data, data_size);
    } else if (RtcDtls::IsDtls((uint8_t*)data, data_size)) {
       uint8_t dtls_data[8*1024];
       int dtls_len = (int)data_size;

       memcpy(dtls_data, data, data_size);

       LogInfof(logger_, "receive dtls data size:%u", data_size);
       dtls_.OnDtlsData(dtls_data, dtls_len);

       return;
    }

    udp_client_->TryRead();
    return;
}

void PeerConnection::HandleRtpData(uint8_t* data, size_t len) {
    if (pc_state_ < PC_DTLS_DONE_STATE) {
        return;
    }
    try {
        if (!read_srtp_) {
            LogErrorf(logger_, "read srtp session is not ready and discard rtp packet");
            return;
        }

        bool ret = read_srtp_->DecryptSrtp(const_cast<uint8_t*>(data), &len);
        if (!ret) {
            LogErrorf(logger_, "decrypt srtp error");
            return;
        }

        RtpPacket* pkt = RtpPacket::Parse(data, len);
        if (!pkt) {
            return;
        }
        uint32_t ssrc = pkt->GetSsrc();
        if (ssrc == video_recv_stream_->GetSsrc()) {
            video_recv_stream_->HandleRtpPacket(pkt);
            jb_video_.InputRtpPacket(video_recv_stream_->GetClockRate(), pkt);
            return;
        } else if (ssrc == audio_recv_stream_->GetSsrc()) {
            audio_recv_stream_->HandleRtpPacket(pkt);
            jb_audio_.InputRtpPacket(audio_recv_stream_->GetClockRate(), pkt);
            return;
        } else {
            LogErrorf(logger_, "fail to find ssrc:%u", ssrc);
        }
    } catch(CppStreamException& e) {
        LogErrorf(logger_, "handle rtp data exception:%s", e.what());
    }
}

void PeerConnection::Report(const std::string& type, const std::string& value) {
    if (!state_report_) {
        return;
    }
    state_report_->OnState(type, value);
}

void PeerConnection::OnDtlsConnected(CRYPTO_SUITE_ENUM suite,
                uint8_t* local_key, size_t local_key_len,
                uint8_t* remote_key, size_t remote_key_len) {
    LogInfoData(logger_, local_key, local_key_len, "on dtls connected srtp local key");

    LogInfoData(logger_, remote_key, remote_key_len, "on dtls connected srtp remote key");

    if (pc_state_ == PC_DTLS_DONE_STATE) {
        LogInfof(logger_, "dtls is connected.");
        return;
    }
    pc_state_ = PC_DTLS_DONE_STATE;

    try {
        if (write_srtp_) {
            delete write_srtp_;
            write_srtp_ = nullptr;
        }
        if (read_srtp_) {
            delete read_srtp_;
            read_srtp_ = nullptr;
        }
        Report("dtls", "ready");
        write_srtp_ = new SRtpSession(SRTP_SESSION_OUT_TYPE, suite, local_key, local_key_len);
        read_srtp_  = new SRtpSession(SRTP_SESSION_IN_TYPE, suite, remote_key, remote_key_len);

        if (direct_type_ == SEND_ONLY) {
            CreateSendStream();
        } else if (direct_type_ == RECV_ONLY) {
            CreateRecvStream();
        } else {
            LogErrorf(logger_, "peer connection direction type error:%d", direct_type_);
        }
    } catch(CppStreamException& e) {
        LogErrorf(logger_, "create srtp session error:%s", e.what());
    }
    return;
}

void PeerConnection::SendRtpPacket(uint8_t* data, size_t len) {
    if(!write_srtp_) {
        LogErrorf(logger_, "write_srtp is not ready");
        return;
    }
    
    bool ret = write_srtp_->EncryptRtp(const_cast<uint8_t**>(&data), &len);
    if (!ret) {
        LogErrorf(logger_, "encrypt_rtp error");
        return;
    }
    udp_client_->Write((char*)data, len, dtls_.remote_address_);
    udp_client_->TryRead();
}

void PeerConnection::SendRtcpPacket(uint8_t* data, size_t len) {
    if(!write_srtp_) {
        return;
    }
    bool ret = write_srtp_->EncryptRtcp(const_cast<uint8_t**>(&data), &len);
    if (!ret) {
        LogErrorf(logger_, "encrypt rtcp error");
        return;
    }
    udp_client_->Write((char*)data, len, dtls_.remote_address_);
    udp_client_->TryRead();
}

void PeerConnection::CreateRecvStream() {
    bool video_nack = answer_sdp_.IsVideoNackEnable();
    bool audio_nack = answer_sdp_.IsAudioNackEnable();

    LogInfof(logger_, "create recv stream video ssrc:%u, audio ssrc:%u",
            answer_sdp_.GetVideoSsrc(), answer_sdp_.GetAudioSsrc());
    if (answer_sdp_.GetVideoSsrc() > 0) {
        has_rtx_ = answer_sdp_.IsVideoRtxEnable();
        uint32_t rtx_ssrc    = answer_sdp_.GetVideoRtxSsrc();
        uint8_t rtx_payload  = answer_sdp_.GetVideoRtxPayloadType();
        if (has_rtx_ && (rtx_payload > 0) && (rtx_ssrc > 0)) {
            video_recv_stream_ = new RtcRecvStream(MEDIA_VIDEO_TYPE, 
                answer_sdp_.GetVideoSsrc(),
                answer_sdp_.GetVideoPayloadType(),
                answer_sdp_.GetVideoClockRate(),
                video_nack, rtx_payload, rtx_ssrc, this,
                logger_, loop_);
           
        } else {
                video_recv_stream_ = new RtcRecvStream(MEDIA_VIDEO_TYPE, 
                answer_sdp_.GetVideoSsrc(),
                answer_sdp_.GetVideoPayloadType(),
                answer_sdp_.GetVideoClockRate(),
                video_nack, this, logger_, loop_);
        }
    }

    if (answer_sdp_.GetAudioSsrc() > 0) {
        audio_recv_stream_ = new RtcRecvStream(MEDIA_AUDIO_TYPE, 
                answer_sdp_.GetAudioSsrc(),
                answer_sdp_.GetAudioPayloadType(),
                answer_sdp_.GetAudioClockRate(),
                audio_nack, this, logger_, loop_);
        audio_recv_stream_->SetChannel(answer_sdp_.channel_);
    }

    return;

}

void PeerConnection::CreateSendStream() {
    bool video_nack = answer_sdp_.IsVideoNackEnable();
    bool audio_nack = answer_sdp_.IsAudioNackEnable();

    LogInfof(logger_, "create send stream video ssrc:%u, audio ssrc:%u",
            answer_sdp_.GetVideoSsrc(), answer_sdp_.GetAudioSsrc());
    if (answer_sdp_.GetVideoSsrc() > 0) {
        has_rtx_ = answer_sdp_.IsVideoRtxEnable();
        uint32_t rtx_ssrc    = answer_sdp_.GetVideoRtxSsrc();
        uint8_t rtx_payload  = answer_sdp_.GetVideoRtxPayloadType();
        if (has_rtx_ && (rtx_payload > 0) && (rtx_ssrc > 0)) {
            video_send_stream_ = new RtcSendStream(MEDIA_VIDEO_TYPE, 
                answer_sdp_.GetVideoSsrc(),
                answer_sdp_.GetVideoPayloadType(),
                answer_sdp_.GetVideoClockRate(),
                video_nack, rtx_payload, rtx_ssrc,
                this, logger_);
           
        } else {
                video_send_stream_ = new RtcSendStream(MEDIA_VIDEO_TYPE, 
                answer_sdp_.GetVideoSsrc(),
                answer_sdp_.GetVideoPayloadType(),
                answer_sdp_.GetVideoClockRate(),
                video_nack, this, logger_);
 
        }
    }

    if (answer_sdp_.GetAudioSsrc() > 0) {
        audio_send_stream_ = new RtcSendStream(MEDIA_AUDIO_TYPE, 
                answer_sdp_.GetAudioSsrc(),
                answer_sdp_.GetAudioPayloadType(),
                answer_sdp_.GetAudioClockRate(),
                audio_nack, this, logger_);
        audio_send_stream_->SetChannel(answer_sdp_.channel_);
    }

    return;
}

void PeerConnection::CreateSendStream2() {
    bool video_nack = offer_sdp_.IsVideoNackEnable();
    bool audio_nack = offer_sdp_.IsAudioNackEnable();

    LogInfof(logger_, "create send stream video ssrc:%u, audio ssrc:%u",
            offer_sdp_.GetVideoSsrc(), offer_sdp_.GetAudioSsrc());
    if (offer_sdp_.GetVideoSsrc() > 0) {
        has_rtx_ = offer_sdp_.IsVideoRtxEnable();
        uint32_t rtx_ssrc    = offer_sdp_.GetVideoRtxSsrc();
        uint8_t rtx_payload  = offer_sdp_.GetVideoRtxPayloadType();
        if (has_rtx_ && (rtx_payload > 0) && (rtx_ssrc > 0)) {
            video_send_stream_ = new RtcSendStream(MEDIA_VIDEO_TYPE, 
                    offer_sdp_.GetVideoSsrc(),
                    offer_sdp_.GetVideoPayloadType(),
                    offer_sdp_.GetVideoClockRate(),
                    video_nack, rtx_payload, rtx_ssrc,
                    this, logger_);
        } else {
             video_send_stream_ = new RtcSendStream(MEDIA_VIDEO_TYPE, 
                    offer_sdp_.GetVideoSsrc(),
                    offer_sdp_.GetVideoPayloadType(),
                    offer_sdp_.GetVideoClockRate(),
                    video_nack, this, logger_);
        }
    }

    if (offer_sdp_.GetAudioSsrc() > 0) {
        audio_send_stream_ = new RtcSendStream(MEDIA_AUDIO_TYPE, 
                offer_sdp_.GetAudioSsrc(),
                offer_sdp_.GetAudioPayloadType(),
                offer_sdp_.GetAudioClockRate(),
                audio_nack, this, logger_);
        audio_send_stream_->SetChannel(offer_sdp_.channel_);
    }
    return;
}

int PeerConnection::SendVideoPacket(Media_Packet_Ptr pkt_ptr) {
    if (!video_send_stream_) {
        return -1;
    }

    video_send_stream_->SendPacket(pkt_ptr);
    return 0;
}

int PeerConnection::SendAudioPacket(Media_Packet_Ptr pkt_ptr) {
    if (!audio_send_stream_) {
        return -1;
    }
    audio_send_stream_->SendPacket(pkt_ptr);
    return 0;
}

void PeerConnection::OnTimer() {
    if (pc_state_ < PC_SDP_DONE_STATE) {
        return;
    }

    int64_t now_ms = now_millisec();

    SendStun(now_ms);

    if (pc_state_ < PC_DTLS_DONE_STATE) {
        return;
    }
    if (video_send_stream_) {
        video_send_stream_->OnTimer(now_ms);
    }

    if (audio_send_stream_) {
        audio_send_stream_->OnTimer(now_ms);
    }

    SendRr(now_ms);
    SendXrDlrr(now_ms);

    OnStatics(now_ms);
}

void PeerConnection::SendRr(int64_t now_ms) {
    RtcpRrPacket rr_pkt;
    RtcpRrBlockInfo* video_rr_block = nullptr;
    RtcpRrBlockInfo* audio_rr_block = nullptr;

    if (video_recv_stream_) {
        video_rr_block = video_recv_stream_->GetRtcpRr(now_ms);
        if (video_rr_block) {
            rr_pkt.AddRrBlock(video_rr_block->GetBlock());
        }
    }
    if (audio_recv_stream_) {
        audio_rr_block = video_recv_stream_->GetRtcpRr(now_ms);
        if (audio_rr_block) {
            rr_pkt.AddRrBlock(audio_rr_block->GetBlock());
        }
    }

    if (video_rr_block || audio_rr_block) {
        size_t data_len;
        uint8_t* data = rr_pkt.GetData(data_len);
        SendRtcpPacket(data, data_len);
    }

    if (video_rr_block) {
        delete video_rr_block;
        video_rr_block = nullptr;
    }

    if (audio_rr_block) {
        delete audio_rr_block;
        audio_rr_block = nullptr;
    }
}

void PeerConnection::SendXrDlrr(int64_t now_ms) {
    XrDlrr dlrr_obj;

    if (direct_type_ != SEND_ONLY) {
        return;
    }
    if (last_send_xr_dlrr_ms_ <= 0) {
        last_send_xr_dlrr_ms_ = now_ms;
        return;
    }

    if (now_ms - last_send_xr_dlrr_ms_ < 1000) {
        return;
    }

    last_send_xr_dlrr_ms_ = now_ms;

    if (last_xr_ms_ <= 0) {
        return;
    }

    if ((now_ms - last_xr_ms_) > 5000) {
        return;
    }

    dlrr_obj.SetSsrc(0x01);
    
    uint32_t lrr = (last_xr_ntp_.ntp_sec & 0xffff) << 16;
    lrr |= (last_xr_ntp_.ntp_frac & 0xffff0000) >> 16;

    int64_t diff_ms = now_ms - last_xr_ms_;
    uint32_t dlrr = (uint32_t)(diff_ms / 1000) << 16;
    dlrr |= (uint32_t)((diff_ms % 1000) * 65536 / 1000);

    if (video_send_stream_) {
        uint32_t rtp_ssrc = video_send_stream_->GetSsrc();
        dlrr_obj.AddrDlrrBlock(rtp_ssrc, lrr, dlrr);
    }

    if (audio_send_stream_) {
        uint32_t rtp_ssrc = audio_send_stream_->GetSsrc();
        dlrr_obj.AddrDlrrBlock(rtp_ssrc, lrr, dlrr);
    }

    SendRtcpPacket(dlrr_obj.GetData(), dlrr_obj.GetDataLen());
}

void PeerConnection::OnStatics(int64_t now_ms) {
    if (pc_state_ < PC_DTLS_DONE_STATE) {
        return;
    }

    if (last_statics_ms_ <= 0) {
        last_statics_ms_ = now_ms;
        return;
    }

    if (last_statics_ms_ + 2000 > now_ms) {
        return;
    }
    last_statics_ms_ = now_ms;

    if (video_send_stream_) {
        size_t vkps = 0;
        size_t vpps = 0;
        int64_t resend_pps = 0;
        int64_t resend_total = 0;
        std::stringstream ss;
        
        resend_total = video_send_stream_->GetResendCount(now_ms, resend_pps);
        video_send_stream_->GetStatics(vkps, vpps);
        ss << "{";
        ss << "\"v_kbps\":" << vkps << ","; 
        ss << "\"v_pps\":" << vpps << ",";
        ss << "\"rtt\":" << video_send_stream_->GetRtt() << ",";
        ss << "\"jitter\":" << video_send_stream_->GetJitter() << ",";
        ss << "\"lost\":" << video_send_stream_->GetLostRate() << ",";
        ss << "\"resend total\":" << resend_total << ",";
        ss << "\"resend pps\":" << resend_pps;
        ss << "}";
        Report("video_statics", ss.str());
    }

    if (audio_send_stream_) {
        size_t akps = 0;
        size_t apps = 0;
        std::stringstream ss;
        audio_send_stream_->GetStatics(akps, apps);
        ss << "{";
        ss << "\"a_kbps\":" << akps << ","; 
        ss << "\"a_pps\":" << apps;
        ss << "\"rtt\":" << audio_send_stream_->GetRtt() << ",";
        ss << "\"jitter\":" << audio_send_stream_->GetJitter() << ",";
        ss << "\"lost\":" << audio_send_stream_->GetLostRate();
        ss << "}";
        Report("audio_statics", ss.str());
    }
}

void PeerConnection::SetRemoteIcePwd(const std::string& ice_pwd) {
    dtls_.remote_pwd_ = ice_pwd;
}

void PeerConnection::SetRemoteIceUserFrag(const std::string& user_frag) {
    dtls_.remote_fragment_ = user_frag;
}

void PeerConnection::SetRemoteUdpAddress(const std::string& ip, uint16_t port) {
    dtls_.remote_address_.ip_address = ip;
    dtls_.remote_address_.port = port;

    IceInfo ice_info;
    ice_info.net_type = ICE_UDP;
    ice_info.hostip   = ip;
    ice_info.port     = port;

    if (dtls_.ice_infos.empty()) {
        dtls_.ice_infos.push_back(ice_info);
    } else {
        bool found = false;
        for (auto& item : dtls_.ice_infos) {
            if (item == ice_info) {
                found = true;
                break;
            }
        }
        if (!found) {
            dtls_.ice_infos.push_back(ice_info);
        }
    }
}

void PeerConnection::SetFingerPrintsSha256(const std::string& sha256_value) {

}

std::string PeerConnection::GetFingerSha256() {
    return dtls_.fingerprint_;
}

void PeerConnection::UpdatePcState(PC_STATE pc_state) {
    pc_state_ = pc_state;
}

int PeerConnection::GetVideoMid() {
    return offer_sdp_.vmid_;
}

int PeerConnection::GetAudioMid() {
    return offer_sdp_.amid_;
}

uint32_t PeerConnection::GetVideoSsrc() {
    return offer_sdp_.video_ssrc_;
}

uint32_t PeerConnection::GetVideoRtxSsrc() {
    return offer_sdp_.video_rtx_ssrc_;
}

uint32_t PeerConnection::GetAudioSsrc() {
    return offer_sdp_.audio_ssrc_;
}

void PeerConnection::SetVideoRtx(bool rtx) {
    auto iter = offer_sdp_.ssrc_info_map_.find(offer_sdp_.video_ssrc_);
    if (iter == offer_sdp_.ssrc_info_map_.end()) {
        CSM_THROW_ERROR("video ssrc is not initied");
    }
    iter->second.is_rtx = rtx;
}

bool PeerConnection::GetVideoRtx() {
    auto iter = offer_sdp_.ssrc_info_map_.find(offer_sdp_.video_rtx_ssrc_);
    if (iter == offer_sdp_.ssrc_info_map_.end()) {
        CSM_THROW_ERROR("video rtx ssrc is not initied");
    }
 
    return iter->second.is_rtx;
}

int PeerConnection::GetVideoPayloadType() {
    for(auto& rtp_item : offer_sdp_.video_rtp_map_infos_) {
        if (rtp_item.second.codec_type != "rtx") {
            return rtp_item.first;
        }
    }
    return -1;
}

int PeerConnection::GetVideoRtxPayloadType() {
    for(auto& rtp_item : offer_sdp_.video_rtp_map_infos_) {
        if (rtp_item.second.codec_type == "rtx") {
            return rtp_item.first;
        }
    }
    return -1;
}

int PeerConnection::GetAudioPayloadType() {
    for(auto& rtp_item : offer_sdp_.audio_rtp_map_infos_) {
        if (rtp_item.second.codec_type != "rtx") {
            return rtp_item.first;
        }
    }
    return -1;
}

std::string PeerConnection::GetVideoCodecType() {
    for(auto& rtp_item : offer_sdp_.video_rtp_map_infos_) {
        if (rtp_item.second.codec_type != "rtx") {
            return rtp_item.second.codec_type;
        }
    }
    return "";
}

std::string PeerConnection::GetAudioCodecType() {
    for(auto& rtp_item : offer_sdp_.audio_rtp_map_infos_) {
        return rtp_item.second.codec_type;
    }
    return "";
}

int PeerConnection::GetVideoClockRate() {
    for(auto& rtp_item : offer_sdp_.video_rtp_map_infos_) {
        if (rtp_item.second.codec_type != "rtx") {
            return rtp_item.second.clock_rate;
        }
    }
    return -1;
}

int PeerConnection::GetAudioClockRate() {
    for(auto& rtp_item : offer_sdp_.audio_rtp_map_infos_) {
        return rtp_item.second.clock_rate;
    }
    return -1;
}


std::vector<RtcpFbInfo> PeerConnection::GetVideoRtcpFbInfo() {
    return offer_sdp_.video_rtcpfb_vec_;
}

std::vector<RtcpFbInfo> PeerConnection::GetAudioRtcpFbInfo() {
    return offer_sdp_.audio_rtcpfb_vec_;
}

void PeerConnection::GetHeaderExternId(int& offset_id, int& abs_send_time_id,
            int& video_rotation_id, int& tcc_id,
            int& playout_delay_id, int& video_content_id,
            int& video_timing_id, int& color_space_id,
            int& sdes_id, int& rtp_streamid_id,
            int& rp_rtp_streamid_id, int& audio_level) {
    offset_id          = offer_sdp_.em_toffset_;
    abs_send_time_id   = offer_sdp_.em_abs_send_time_;
    video_rotation_id  = offer_sdp_.em_video_rotation_;
    tcc_id             = offer_sdp_.em_tcc_;
    playout_delay_id   = offer_sdp_.em_playout_delay_;
    video_content_id   = offer_sdp_.em_video_content_;
    video_timing_id    = offer_sdp_.em_video_timing_;
    color_space_id     = offer_sdp_.em_color_space_;
    sdes_id            = offer_sdp_.em_sdes_;
    rtp_streamid_id    = offer_sdp_.em_rtp_streamid_;
    rp_rtp_streamid_id = offer_sdp_.em_rp_rtp_streamid_;
    audio_level        = offer_sdp_.em_audio_level_;
}


std::string PeerConnection::GetVideoCName() {
    return offer_sdp_.video_cname_;
}

std::string PeerConnection::GetAudioCName() {
    return offer_sdp_.audio_cname_;
}

void PeerConnection::RtpPacketReset(std::shared_ptr<RtpPacketInfo> pkt_ptr) {

}

void PeerConnection::RtpPacketOutput(std::shared_ptr<RtpPacketInfo> pkt_ptr) {

}

}
