#include "audio_header.hpp"
#include "flv_pub.hpp"

#include <cstring>
#include <stdio.h>

namespace cpp_streamer
{

static const int mpeg4audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

static const int flv_audio_rates[4] = {
    5512, 11025, 22050, 44100
};

bool GetAudioInfoByFlvHeader(uint8_t flag,
                       MEDIA_CODEC_TYPE& codec_type, 
                       int& sample_rate, 
                       int& sample_size,
                       uint8_t& channel) {
    switch (flag & FLV_AUDIO_CODECID_MASK)
    {
        case FLV_AUDIO_AAC_CODEC:
            codec_type = MEDIA_CODEC_AAC;
            break;
        case FLV_AUDIO_MP3_CODEC:
            codec_type = MEDIA_CODEC_MP3;
            break;
        case FLV_AUDIO_OPUS_CODEC:
            codec_type = MEDIA_CODEC_OPUS;
            break;
        default:
            return false;
    }
    uint8_t rate_flag = (flag & FLV_AUDIO_SAMPLERATE_MASK) >> 2;

    sample_rate = flv_audio_rates[rate_flag];
    sample_size = (flag & FLV_AUDIO_SAMPLESIZE_MASK) ? FLV_16bits_SAMPLE_SIZE : FLV_8bits_SAMPLE_SIZE;
    channel = ((flag & FLV_AUDIO_CHANNEL_MASK) == FLV_AUDIO_CHANNEL_MASK) ? 2 : 1;

    return true;
}

int GetSampleRateByIndex(int index) {
    if ((size_t)index >= sizeof(mpeg4audio_sample_rates)/sizeof(int)) {
        return -1;
    }
    return mpeg4audio_sample_rates[index];
}

int GetSamplerateIndex(int samplerate) {
    int index = 0;//default 44100
    bool found = false;
    for (; index < (int)(sizeof(mpeg4audio_sample_rates)/sizeof(int)); index++) {
        if (samplerate == mpeg4audio_sample_rates[index]) {
            found = true;
            break;
        }
    }
    if (!found) {
        index = 4;
    }
    return index;
}

int GetAdtsTypeByAscType(int asc_type) {
    switch (asc_type) {
        case ASC_TYPE_AAC_MAIN: return AAC_ADTS_TYPE_MAIN;
        case ASC_TYPE_AAC_HE:
        case ASC_TYPE_AAC_HEv2:
        case ASC_TYPE_AAC_LC: return AAC_ADTS_TYPE_LC;
        case ASC_TYPE_AAC_SSR: return AAC_ADTS_TYPE_SSR;
        default: return AAC_ADTS_TYPE_RESERVE;
    }
}

int GetAscTypeByAdtsType(int adts_type) {
    switch (adts_type) {
        case AAC_ADTS_TYPE_MAIN: return ASC_TYPE_AAC_MAIN;
        case AAC_ADTS_TYPE_LC: return ASC_TYPE_AAC_LC;
        case AAC_ADTS_TYPE_SSR: return ASC_TYPE_AAC_SSR;
        default: return ASC_TYPE_AAC_LC;
    }
}
//xxxx xyyy yzzz z000
//aac type==5(or 29): xxx xyyy yzzz zyyy yzzz z000
/*
    xxxxx: audioObjectType;
    yyyy: samplingFrequencyIndex;
    if (samplingFrequencyIndex == 0xf)
        samplingFrequency: 24bits
    zzzz: channelConfiguration;
*/

bool GetAudioInfoByAsc(uint8_t* data, size_t data_size,
                       uint8_t& audio_type, int& sample_rate,
                       uint8_t& channel) {
    uint8_t sample_rate_index = 0;
    uint8_t* p = data;
    int left_len = (int)data_size;
    uint16_t field = 0;
    uint8_t extensionAudioObjectType = 0;
    
    assert(data_size >= 2);

    field = ByteStream::Read2Bytes(p);
    p++;
    left_len -= 2;

    audio_type = (field & 0xf900) >> 11;
    if (audio_type == 31) {
        // aaaa abbb bbbc cccd
        audio_type = (field & 0x07e0) >> 5;//aaaa abbb bbb
        sample_rate_index = (field & 0x1e) >> 1;//cccc

        if (sample_rate_index == 0x0f) {
            uint8_t last_bit = field & 0x01;
            // cccc = 0x0f
            // aaaa abbb bbbc cccd dddd dddd dddd dddd dddd ddde eeef ffff
            assert(left_len > 3);
            field = ByteStream::Read3Bytes(p);
            p += 3;
            left_len -= 3;

            //d dddd dddd dddd dddd ddde
            sample_rate = ((uint32_t)last_bit) >> 24;
            sample_rate |= field >> 1;

            last_bit = field & 0x01;
            //eeee
            channel = (last_bit << 3);
            channel |= (*p >> 5) & 0x07;
        } else {
            // aaaa abbb bbbc cccd ddd
            uint8_t last_bit = field & 0x01;
            channel = last_bit << 3;
            channel |= ((*p) >> 5) & 0x07;

            sample_rate = mpeg4audio_sample_rates[sample_rate_index];
        }
    } else {
        // aaaa abbb bxxx xxxx
        if (audio_type != 5 && audio_type != 29) {
            extensionAudioObjectType = 0;
        }
        sample_rate_index = ((field & 0x0790) >> 7) & 0x0f;// bbbb

        if (sample_rate_index == 0x0f) {
            // aaaa abbb bccc cccc cccc cccc cccc cccx
            uint32_t last_bits = field & 0x7f;
            field = ByteStream::Read2Bytes(p);
            p += 2;
            left_len -= 2;

            sample_rate = (last_bits << 17) & 0xfe0000;
            sample_rate |= (field >> 1) & 0x7f;
        } else {
            // aaaa abbb bccc cxxx
            channel = (field >> 3) & 0x0f;

            sample_rate = mpeg4audio_sample_rates[sample_rate_index];
        }
    }
    (void)extensionAudioObjectType;
    
    return true;
}

bool GetAudioInfo2ByAsc(uint8_t* data, size_t data_size,
                        uint8_t& audio_type, int& sample_rate,
                        uint8_t& channel) {
    uint8_t sample_rate_index = 0;
    uint8_t* p = data;

    audio_type = (*p >> 3) & 0x1F;
    if (audio_type == 31) {
        return false;//not supported in the version
    }

    sample_rate_index = (*p & 0x07) << 1;
    p++;
    sample_rate_index |= (*p & 0x80) >> 7;
    if (sample_rate_index > sizeof(mpeg4audio_sample_rates)) {
        return false;//not supported in the version
    }

    sample_rate = mpeg4audio_sample_rates[sample_rate_index];
    channel = (*p >> 3) & 0x0F;
    
    if ((audio_type == 5) || (audio_type == 29)) {
        sample_rate_index = ((p[0] << 1) & 0x0E) | (p[1] >> 7);
        sample_rate = mpeg4audio_sample_rates[sample_rate_index];
    }
    return true;
}

/*
    ADTS HEADER: 7 Bytes. See ISO 13818-7 (2004)
    AAAAAAAA AAAABCCD EEFFFFGH HHIJKLMM MMMMMMMM MMMOOOOO OOOOOOPP
    A - Sync 0xFFFx
    B   1   MPEG Version: 0 for MPEG-4, 1 for MPEG-2
    C   2   Layer: always 0
    D   1   protection absent, Warning, set to 1 if there is no CRC and 0 if there is CRC
    E   2   profile, the MPEG-4 Audio Object Type minus 1
    F   4   MPEG-4 Sampling Frequency Index (15 is forbidden)
    G   1   private bit, guaranteed never to be used by MPEG, set to 0 when encoding, ignore when decoding
    H   3   MPEG-4 Channel Configuration (in the case of 0, the channel configuration is sent via an inband PCE)
    I   1   originality, set to 0 when encoding, ignore when decoding
    J   1   home, set to 0 when encoding, ignore when decoding
    K   1   copyrighted id bit, the next bit of a centrally registered copyright identifier, set to 0 when encoding, ignore when decoding
    L   1   copyright id start, signals that this frame's copyright id bit is the first bit of the copyright id, set to 0 when encoding, ignore when decoding
    M   13  frame length, this value must include 7 or 9 bytes of header length: FrameLength = (ProtectionAbsent == 1 ? 7 : 9) + size(AACFrame)
    O   11  Buffer fullness
    P   2   Number of AAC frames (RDBs) in ADTS frame minus 1, for maximum compatibility always use 1 AAC frame per ADTS frame
    Q   16  CRC if protection absent is 0
*/
int MakeAdts(uint8_t* data, uint8_t object_type,
             int sample_rate, int channel, 
             int full_frame_size, bool mpegts2) {
    const int ADTS_LEN = 7;
    int sample_rate_index = GetSamplerateIndex(sample_rate);
    full_frame_size += ADTS_LEN;
    full_frame_size &= 0x1FFF;
    
    //AAAAAAAA AAAABCCD EEFFFFGH HHIJKLMM MMMMMMMM MMMOOOOO OOOOOOPP
    // - Sync 0xFFFx

    //first write adts header
    data[0] = 0xff;
    if (mpegts2) {
        data[1] = 0xf9;
    } else {
        data[1] = 0xf1;
    }
    
    data[2] = 0x00;
    data[2] = data[2] | ((object_type - 1)<<6);
    data[2] = data[2] | ((sample_rate_index)<<2);
    data[2] = data[2] | ((channel >> 2));
    printf("obj type:%d, rate index:%d, channel:%d, data[2]:0x%02x, rate:%d.\r\n",
            object_type, sample_rate_index, channel, data[2], sample_rate);
    //0x80
    data[3] &= 0x00;
    data[3] = data[3] | (channel << 6);
    data[3] = data[3] | ((full_frame_size<<3)>>14);
    
    data[4] &= 0x00;
    data[4] = data[4] | ((full_frame_size<<5)>>8);
    
    data[5] &= 0x00;
    data[5] = data[5] | (((full_frame_size<<13)>>13)<<5);
    data[5] = data[5] | ((0x7C<<1)>>3);
    data[6] = 0xfc;
    
    return ADTS_LEN;
}

size_t MakeOpusHeader(uint8_t* data, int sample_rate, int channel) {
    uint8_t* p = data;
    const char opus_str[] = "OpusHead";
    const size_t opus_str_len = 8;
    size_t header_len = 0;

    memcpy(p, (uint8_t*)opus_str, opus_str_len);
    p += opus_str_len;

    *p = 1;
    p++;
    *p = (uint8_t)channel;
    p++;
    ByteStream::Write2Bytes_le(p, 0);//initial_padding
    p += 2;
    ByteStream::Write4Bytes_le(p, sample_rate);
    p += 4;
    ByteStream::Write2Bytes_le(p, 0);
    p += 2;
    *p = 0;//mapping_family
    p++;
/*
{ 0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64,
  0x01, 0x02, 0x38, 0x01, 0x80, 0xbb, 0x00, 0x00,
  0x00, 0x00, 0x00}
*/
    header_len = (size_t)(p - data);
    return header_len;
}
}
