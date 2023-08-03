#include "rtc_send_stream.hpp"
#include "h264_h265_header.hpp"
#include "rtp_h264_pack.hpp"
#include "opus_header.hpp"

#include "timeex.hpp"

namespace cpp_streamer
{

#define SEND_BUFFER_SIZE 2048

RtcSendStream::RtcSendStream(MEDIA_PKT_TYPE type, 
            uint32_t ssrc, uint8_t payload, 
            int clock_rate, bool nack, 
            RtcSendStreamCallbackI* cb, Logger* logger):logger_(logger)
                        , media_type_(type)
                        , cb_(cb)
{
    ssrc_        = ssrc;
    pt_          = payload;
    clock_rate_  = clock_rate;
    nack_enable_ = nack;
    has_rtx_     = false;

    last_sr_ntp_ts_ = {
        .ntp_sec  = 0,
        .ntp_frac = 0
    };

    send_buffer_.resize(SEND_BUFFER_SIZE);
    for (auto& item : send_buffer_) {
        item.last_ms = 0;
        item.retry_count = 0;
        item.pkt = nullptr;
    }
    LogInfof(logger, "RtcSendStream construct type:%s, ssrc:%u, payload:%d, clock rate:%d, nack:%s, rtx disable",
            type == MEDIA_VIDEO_TYPE ? "video" : "audio",
            ssrc, payload, clock_rate,
            nack ? "enable" : "disable");
}

RtcSendStream::RtcSendStream(MEDIA_PKT_TYPE type, 
            uint32_t ssrc, uint8_t payload, int clock_rate,
            bool nack, uint8_t rtx_payload, uint32_t rtx_ssrc,
            RtcSendStreamCallbackI* cb, Logger* logger):logger_(logger)
                                                    , media_type_(type)
                                                    , cb_(cb)
{
    ssrc_        = ssrc;
    pt_          = payload;
    clock_rate_  = clock_rate;
    nack_enable_ = nack;
    has_rtx_     = true;
    rtx_payload_ = rtx_payload;
    rtx_ssrc_    = rtx_ssrc;

    last_sr_ntp_ts_ = {
        .ntp_sec  = 0,
        .ntp_frac = 0
    };

    send_buffer_.resize(SEND_BUFFER_SIZE);
    for (auto& item : send_buffer_) {
        item.last_ms = 0;
        item.retry_count = 0;
        item.pkt = nullptr;
    }
    LogInfof(logger, "RtcSendStream construct type:%s, ssrc:%u, payload:%d, clock rate:%d, nack:%s, rtx enable, rtx payload:%d, rtx ssrc:%u",
            type == MEDIA_VIDEO_TYPE ? "video" : "audio",
            ssrc, payload, clock_rate,
            nack ? "enable" : "disable",
            rtx_payload, rtx_ssrc);

}

RtcSendStream::~RtcSendStream()
{
    LogInfof(logger_, "destruct RtcSendStream %s", avtype_tostring(media_type_).c_str());

    for (auto& item : send_buffer_) {
        item.last_ms     = 0;
        item.retry_count = 0;
        if(item.pkt) {
            delete item.pkt;
            item.pkt = nullptr;
        }
    }
}

void RtcSendStream::SendPacket(Media_Packet_Ptr pkt_ptr) {
    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        SendVideoPacket(pkt_ptr);
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        if (pkt_ptr->is_seq_hdr_) {
            uint8_t* data = (uint8_t*)pkt_ptr->buffer_ptr_->Data();
            uint8_t len   = pkt_ptr->buffer_ptr_->DataLen();
            LogInfoData(logger_, data, len, "opus extra");
        }
        SendAudioPacket(pkt_ptr);
    } else {
        LogErrorf(logger_, "media packet av type:%d is not supported", pkt_ptr->av_type_);
    }
}

void RtcSendStream::SendVideoPacket(Media_Packet_Ptr pkt_ptr) {
    if (pkt_ptr->codec_type_ == MEDIA_CODEC_H264) {
        SendH264Packet(pkt_ptr);
    }
}

void RtcSendStream::SendAudioPacket(Media_Packet_Ptr pkt_ptr) {
    uint8_t* data = (uint8_t*)pkt_ptr->buffer_ptr_->Data();
    uint8_t len   = pkt_ptr->buffer_ptr_->DataLen();
    int64_t ts    = pkt_ptr->dts_;
    ts = ts * clock_rate_ / 1000;

    RtpPacket* pkt = GenerateSinglePackets(data, len);

    pkt->SetPayloadType(pt_);
    pkt->SetSsrc(ssrc_);
    pkt->SetSeq(seq_++);
    pkt->SetTimestamp((uint32_t)ts);
    pkt->SetMarker(1);
    //LogInfof(logger_, "send audio packet:%s",
    //        pkt->Dump().c_str());

    SendAudioRtpPacket(pkt);

    delete pkt;
}

void RtcSendStream::SendH264Packet(Media_Packet_Ptr pkt_ptr) {
    uint8_t* data = (uint8_t*)pkt_ptr->buffer_ptr_->Data();
    size_t len    = pkt_ptr->buffer_ptr_->DataLen();
    int64_t ts    = pkt_ptr->dts_;
    ts = ts * clock_rate_ / 1000;

    int pos = GetNaluTypePos(data);
    if (H264_IS_SEI(data[pos])) {
        LogInfof(logger_, "skip h264 sei packet len:%lu, clock rate:%d, pos:%d", 
                len, clock_rate_, pos);
        return;
    }
    if (H264_IS_KEYFRAME(data[pos])) {
        LogDebugf(logger_, "send h264 keyframe len:%lu, pos:%d", len, pos);
    }

    pkt_ptr->buffer_ptr_->ConsumeData(pos);
    data = (uint8_t*)pkt_ptr->buffer_ptr_->Data();
    len  = pkt_ptr->buffer_ptr_->DataLen();
    //LogInfof(logger_, "h264 data:0x%02x",
    //        data[0], data[1], data[2], data[3], data[4]);
    if (pkt_ptr->is_seq_hdr_) {
        if (len >= sizeof(sps_)) {
            LogErrorf(logger_, "nalu sps/pps len:%lu error", len);
            return;
        }
        uint8_t nalu_type = data[0] & 0x1f;

        if (H264_IS_SPS(nalu_type)) {
            sps_len_ = len;
            memcpy(sps_, data, sps_len_);
        }
        if (H264_IS_PPS(nalu_type)) {
            pps_len_ = len;
            memcpy(pps_, data, pps_len_);
        }
        return;
    }

    if (pkt_ptr->is_key_frame_) {
        if (sps_len_ > 0 && pps_len_ > 0) {
            std::vector<std::pair<uint8_t*, int>> data_vec;

            data_vec.push_back({sps_, sps_len_});
            data_vec.push_back({pps_, pps_len_});           

            //StapA packet
            RtpPacket* pkt = GenerateStapAPackets(data_vec);
            pkt->SetPayloadType(pt_);
            pkt->SetSsrc(ssrc_);
            pkt->SetSeq(seq_++);
            pkt->SetTimestamp((uint32_t)ts);
            pkt->SetMarker(0);

            SendVideoRtpPacket(pkt);
            delete pkt;
        }
    }

    //single packet
    if (len <= kPayloadMaxSize) {
        RtpPacket* pkt = GenerateSinglePackets(data, len);

        pkt->SetPayloadType(pt_);
        pkt->SetSsrc(ssrc_);
        pkt->SetSeq(seq_++);
        pkt->SetTimestamp((uint32_t)ts);
        pkt->SetMarker(1);

        SendVideoRtpPacket(pkt);
        delete pkt;
        return;
    }

    //fuA packet
    std::vector<RtpPacket*> fuA_vec = GenerateFuAPackets(data, len);
    for (auto fuA_pkt : fuA_vec) {
        fuA_pkt->SetPayloadType(pt_);
        fuA_pkt->SetSsrc(ssrc_);
        fuA_pkt->SetSeq(seq_++);
        fuA_pkt->SetTimestamp((uint32_t)ts);

        SendVideoRtpPacket(fuA_pkt);
        delete fuA_pkt;
    }
    return;
}

void RtcSendStream::SendVideoRtpPacket(RtpPacket* pkt, bool resend) {
    sent_count_++;
    sent_bytes_ += pkt->GetDataLength();

    if (last_sr_ts_ == 0) {
        last_sr_ts_ = now_millisec();
        RtcpSrPacket* sr = GetRtcpSr(last_sr_ts_);
        cb_->SendRtcpPacket(sr->GetData(), sr->GetDataLen());
        delete sr;
    }
    if (!resend) {
        SaveBuffer(pkt);
    }
    statics_.Update(pkt->GetDataLength(), now_millisec());
    cb_->SendRtpPacket(pkt->GetData(), pkt->GetDataLength());
}

void RtcSendStream::SendAudioRtpPacket(RtpPacket* pkt) {
    sent_count_++;
    sent_bytes_ += pkt->GetDataLength();

    if (last_sr_ts_ == 0) {
        last_sr_ts_ = now_millisec();
        RtcpSrPacket* sr = GetRtcpSr(last_sr_ts_);
        cb_->SendRtcpPacket(sr->GetData(), sr->GetDataLen());
        delete sr;
    }
    statics_.Update(pkt->GetDataLength(), now_millisec());
    cb_->SendRtpPacket(pkt->GetData(), pkt->GetDataLength());
}

void RtcSendStream::OnTimer(int64_t now_ts) {
    if (now_ts - last_sr_ts_ > 500) {
        last_sr_ts_ = now_ts;
        RtcpSrPacket* sr = GetRtcpSr(last_sr_ts_);
        cb_->SendRtcpPacket(sr->GetData(), sr->GetDataLen());
        delete sr;
    }
}

RtcpSrPacket* RtcSendStream::GetRtcpSr(int64_t now_ms) {
    RtcpSrPacket* sr_pkt = new RtcpSrPacket();

    last_sr_ntp_ts_ = millisec_to_ntp(now_ms);
    last_sr_rtp_ts_ = (uint32_t)(now_ms / 1000 * clock_rate_);

    sr_pkt->SetSsrc(ssrc_);
    sr_pkt->SetNtp(last_sr_ntp_ts_.ntp_sec, last_sr_ntp_ts_.ntp_frac);
    sr_pkt->SetRtpTimestamp(last_sr_rtp_ts_);
    sr_pkt->SetPktCount(sent_count_);
    sr_pkt->SetBytesCount(sent_bytes_);

    return sr_pkt;
}

void RtcSendStream::SaveBuffer(RtpPacket* pkt) {
    if (nack_enable_) {
        size_t index = pkt->GetSeq() % SEND_BUFFER_SIZE;
        SendRtpPacketInfo& info = send_buffer_[index];
        if (info.pkt) {
            delete info.pkt;
            info.pkt = nullptr;
        }
        info.pkt         = pkt->Clone(nullptr);
        info.retry_count = 0;
        info.last_ms     = 0;
    }
}

void RtcSendStream::ResendRtpPacket(uint16_t seq) {
    size_t index = seq % SEND_BUFFER_SIZE;

    SendRtpPacketInfo& info = send_buffer_[index];
    if (!info.pkt) {
        LogWarnf(logger_, "fail to find rtp packet by seq:%d, index:%lu", seq, index);
        return;
    }
    if (info.pkt->GetSeq() != seq) {
        LogWarnf(logger_, "fail to get rtp packet(%d) by seq:%d, index:%lu", 
                seq, info.pkt->GetSeq(), index);
        return;
    }

    int64_t now_ms = now_millisec();
    if (info.last_ms == 0) {
        info.retry_count = 1;
        info.last_ms     = now_ms;
    } else {
        float interval = avg_rtt_ > 10 ? avg_rtt_ / 2 : avg_rtt_;
        if ((float)(now_ms - info.last_ms + 10) < interval) {
            LogDebugf(logger_, "resend too often and ignore nack request, interval:%ld, rtt:%f",
                    now_ms - info.last_ms, avg_rtt_);
            return;
        }
        info.retry_count++;
        if (info.retry_count > 5) {
            LogInfof(logger_, "resend times(%d) is too large, seq:%d", info.retry_count,seq);
        }
    }
    info.last_ms = now_ms;

    resend_cnt_++;
    LogDebugf(logger_, "resend packet seq:%d, retry count:%d",
            seq, info.retry_count);
    if (!has_rtx_) {
        SendVideoRtpPacket(info.pkt, true);
        return;
    }
    RtpPacket* rtx_pkt = info.pkt->Clone();
    rtx_pkt->RtxMux(rtx_payload_, rtx_ssrc_, rtx_seq_++);
    SendVideoRtpPacket(rtx_pkt, true);
    delete rtx_pkt;
    return;
}

void RtcSendStream::HandleRtcpRr(RtcpRrBlockInfo& block) {
    lost_total_ = block.GetCumulativeLost();
    uint8_t frac_lost = block.GetFracLost();
    lost_rate_ = (float)(frac_lost/256.0);
    jitter_ = block.GetJitter();

    //RTT= RTP发送方本地时间 - RR中LSR - RR中DLSR
    uint32_t lsr = block.GetLsr();
    uint32_t dlsr = block.GetDlsr();

    if (lsr == 0) {
        rtt_ = RTT_DEFAULT;
        return;
    }
    int64_t now_ms = (int64_t)now_millisec();

    NTP_TIMESTAMP now_ntp = millisec_to_ntp(now_ms);
    uint32_t compact_ntp = (now_ntp.ntp_sec & 0x0000FFFF) << 16;

    compact_ntp |= (now_ntp.ntp_frac & 0xFFFF0000) >> 16;

    uint32_t rtt = 0;

    // If no Sender Report was received by the remote endpoint yet, ignore lastSr
    // and dlsr values in the Receiver Report.
    if (lsr && dlsr && (compact_ntp > dlsr + lsr)){
        rtt = compact_ntp - dlsr - lsr;
    }
    rtt_ = static_cast<float>(rtt >> 16) * 1000.0;
    rtt_ += (static_cast<float>(rtt & 0x0000FFFF) / 65536.0) * 1000;

    avg_rtt_ += (rtt_ - avg_rtt_) / 4.0;

    LogDebugf(logger_, "handle rtcp rr media(%s), ssrc:%u, lost total:%u, lost rate:%.03f, jitter:%u, rtt_:%.02f, avg rtt:%.02f",
            avtype_tostring(media_type_).c_str(), ssrc_, lost_total_, lost_rate_, jitter_, rtt_, avg_rtt_);

}

void RtcSendStream::HandleRtcpNack(RtcpFbNack* nack_pkt) {
    std::vector<uint16_t> lost_seqs = nack_pkt->GetLostSeqs();

    std::stringstream ss;
    ss << "[";
    for (auto seq : lost_seqs) {
        ss << " " << seq;
    }
    ss << " ]";
    LogInfof(logger_, "media ssrc:%u, type:%s, nack lost seqs:%s, avg rtt:%.02f, nack:%s",
        nack_pkt->GetMediaSsrc(), avtype_tostring(media_type_).c_str(),
        ss.str().c_str(), avg_rtt_, nack_enable_ ? "enable" : "disable");

    if (!nack_enable_) {
        return;
    }
    for (auto seq : lost_seqs) {
        ResendRtpPacket(seq);
    }
}

void RtcSendStream::HandleXrRrt(XrRrtData* rrt_block) {

}

void RtcSendStream::GetStatics(size_t& kbits, size_t& pps) {
    int64_t now_ms = now_millisec();

    kbits = statics_.BytesPerSecond(now_ms, pps) * 8 / 1000;
}

int64_t RtcSendStream::GetResendCount(int64_t now_ms, int64_t& resend_pps) {
    if (last_resend_statics_ms_ <= 0) {
        last_resend_statics_ms_ = now_ms;
        last_resend_cnt_ = resend_cnt_;
        resend_pps = 0;
        return resend_cnt_;
    }
    int64_t diff_ms = now_ms - last_resend_statics_ms_;
    last_resend_statics_ms_ = now_ms;

    if (diff_ms > 0) {
        resend_pps = (resend_cnt_ - last_resend_cnt_) * 1000 / diff_ms;
    } else {
        resend_pps = 0;
    }

    last_resend_cnt_ = resend_cnt_;
    return resend_cnt_;
}

}
