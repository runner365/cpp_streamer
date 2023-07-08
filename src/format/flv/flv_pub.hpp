#ifndef FLV_PUB_HPP
#define FLV_PUB_HPP
#include "media_packet.hpp"
#include "logger.hpp"

namespace cpp_streamer
{
#define FLV_TAG_AUDIO     0x08
#define FLV_TAG_VIDEO     0x09
#define FLV_TAG_TYPE_META 0x12

#define FLV_VIDEO_KEY_FLAG   0x10
#define FLV_VIDEO_INTER_FLAG 0x20

#define FLV_VIDEO_AVC_SEQHDR 0x00
#define FLV_VIDEO_AVC_NALU   0x01

#define FLV_VIDEO_H264_CODEC 0x07
#define FLV_VIDEO_H265_CODEC 0x0c
#define FLV_VIDEO_AV1_CODEC  0x0d
#define FLV_VIDEO_VP8_CODEC  0x0e
#define FLV_VIDEO_VP9_CODEC  0x0f

#define FLV_AUDIO_MP3_CODEC  0x20
#define FLV_AUDIO_OPUS_CODEC  0x90
#define FLV_AUDIO_AAC_CODEC   0xa0

/* offsets for packed values */
#define FLV_AUDIO_SAMPLESSIZE_OFFSET 1
#define FLV_AUDIO_SAMPLERATE_OFFSET  2
#define FLV_AUDIO_CODECID_OFFSET     4

enum {
    FLV_MONO   = 0,
    FLV_STEREO = 1,
};

enum {
    FLV_SAMPLESSIZE_8BIT  = 0,
    FLV_SAMPLESSIZE_16BIT = 1 << FLV_AUDIO_SAMPLESSIZE_OFFSET,
};

enum {
    FLV_SAMPLERATE_SPECIAL = 0, /**< signifies 5512Hz and 8000Hz in the case of NELLYMOSER */
    FLV_SAMPLERATE_11025HZ = 1 << FLV_AUDIO_SAMPLERATE_OFFSET,
    FLV_SAMPLERATE_22050HZ = 2 << FLV_AUDIO_SAMPLERATE_OFFSET,
    FLV_SAMPLERATE_44100HZ = 3 << FLV_AUDIO_SAMPLERATE_OFFSET,
};

/*
ASC flag：xxxx xyyy yzzz z000
x： aac type，类型2表示AAC-LC，5是SBR, 29是ps，5和29比较特殊ascflag的长度会变成4；
y:  sample rate, 采样率, 7表示22050采样率
z:  通道数，2是双通道
*/
inline void flv_audio_asc_decode(char* data, uint8_t& audio_type, uint8_t& sample_rate_index, uint8_t& channel) {
//2    b    9    2    0    8    0    0
//0010 1011 1001 0010 0000 1000 0000 0000
//aac type: 5
// samplerate index: 7
}

inline int AddFlvMediaHeader(Media_Packet_Ptr pkt_ptr, Logger* logger) {
    uint8_t* p;

    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
    if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        p = (uint8_t*)pkt_ptr->buffer_ptr_->ConsumeData(-2);

        if (pkt_ptr->codec_type_ == MEDIA_CODEC_AAC) {
            p[0] = FLV_AUDIO_AAC_CODEC | 0x0f;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_OPUS) {
            p[0] = FLV_AUDIO_OPUS_CODEC | 0x0f;
        } else {
            LogErrorf(logger, "unsuport audio codec type:%d", pkt_ptr->codec_type_);
            return -1;
        }

        if (pkt_ptr->is_seq_hdr_) {
            p[1] = 0x00;
        } else {
            p[1] = 0x01;
        }
    } else if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        p = (uint8_t*)pkt_ptr->buffer_ptr_->ConsumeData(-5);

        p[0] = 0;
        if (pkt_ptr->codec_type_ == MEDIA_CODEC_H264) {
            p[0] |= FLV_VIDEO_H264_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_H265) {
            p[0] |= FLV_VIDEO_H265_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_VP8) {
            p[0] |= FLV_VIDEO_VP8_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_VP9) {
            p[0] |= FLV_VIDEO_VP9_CODEC;
        }  else {
            LogErrorf(logger, "unsuport video codec type:%d", pkt_ptr->codec_type_);
            return -1;
        }

        if (pkt_ptr->is_key_frame_ || pkt_ptr->is_seq_hdr_) {
            p[0] |= FLV_VIDEO_KEY_FLAG;
            if (pkt_ptr->is_seq_hdr_) {
                p[1] = 0x00;
            } else {
                p[1] = 0x01;
            }
        } else {
            p[0] |= FLV_VIDEO_INTER_FLAG;
            p[1] = 0x01;
        }
        uint32_t ts_delta = (pkt_ptr->pts_ > pkt_ptr->dts_) ? (pkt_ptr->pts_ - pkt_ptr->dts_) : 0;
        p[2] = (ts_delta >> 16) & 0xff;
        p[3] = (ts_delta >> 8) & 0xff;
        p[4] = ts_delta & 0xff;
    }

    return 0;

}

}

#endif

