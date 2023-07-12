#include "opus_header.hpp"
#include "byte_stream.hpp"
#include <cstring>
#include <stdio.h>
#include <vector>
#include <map>

namespace cpp_streamer
{
#ifdef __linux__
static const int INT_MAX = 0x7fffffff;
#endif
/*
static const uint8_t opus_coupled_stream_cnt[9] = {
    1, 0, 1, 1, 2, 2, 2, 3, 3
};

static const uint8_t opus_stream_cnt[9] = {
    1, 1, 1, 2, 2, 3, 4, 4, 5,
};
static const uint8_t opus_channel_map[8][8] = {
    { 0 },
    { 0,1 },
    { 0,2,1 },
    { 0,1,2,3 },
    { 0,4,1,2,3 },
    { 0,4,1,2,3,5 },
    { 0,4,1,2,3,5,6 },
    { 0,6,1,2,3,4,5,7 },
};

static const uint8_t opus_default_extradata[30] = {
    'O', 'p', 'u', 's', 'H', 'e', 'a', 'd',
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
*/

/*
 * Codec homepage: http://opus-codec.org/
 * Specification: http://tools.ietf.org/html/rfc6716
 * Ogg Opus specification: https://tools.ietf.org/html/draft-ietf-codec-oggopus-03
 */

/*
 *                      Table 6-1 opus_access_unit syntax

 |Syntax                               |Number of  |Identif|
 |                                     |bits       |ier    |
 |opus_access_unit() {                 |           |       |
 |   if(nextbits(11)==0x3FF) {         |           |       |
 |      opus_control_header()          |           |       |
 |                                     |           |       |
 |      for(i=0; i<stream_count-1; i++)|           |       |
 |{                                    |           |       |
 |          self_delimited_opus_packet |           |       |
 |      }                              |           |       |
 |undelimited_opus_packet              |           |       |
 |}                                    |           |       |
 *
 *
 *                      Table 6-2 opus_access_unit syntax

 |Syntax                                         |Number of  |Identif|
 |                                               |bits       |ier    |
 |opus_control_header() {                        |           |       |
 |   control_header_prefix                       |11         |bslbf  |
 |   start_trim_flag                             |1          |bslbf  |
 |   end_trim_flag                               |1          |bslbf  |
 |   control_extension_flag                      |1          |bslbf  |
 |   Reserved                                    |2          |bslbf  |
 |   au_size = 0                                 |           |       |
 |while(nextbits(8) == 0xFF){                    |           |       |
 |ff_byte [= 0xFF]                               |8          |uimsbf |
 |au_size += 255;                                |           |       |
 |}                                              |           |       |
 |au_size_last_byte                              |8          |uimsbf |
 |au_size += au_size_last_byte                   |           |       |
 |if(start_trim_flag==1) {                       |           |       |
 |      Reserved                                 |3          |bslbf  |
 |      start_trim                               |13         |uimsbf |
 |   }                                           |           |       |
 |   if(end_trim_flag==1) {                      |           |       |
 |      Reserved                                 |3          |bslbf  |
 |      end_trim                                 |13         |uimsbf |
 |   }                                           |           |       |
 |   if(control_extension_flag==1) {             |           |       |
 |      control_extension_length                 |8          |uimsbf |
 |      for(i=0; i<control_extension_length; i++)|           |       |
 |{                                              |           |       |
 |         reserved                              |8          |bslbf  |
 |      }                                        |           |       |
 |   }                                           |           |       |
 |}                                              |           |       |
 */
static uint8_t* GetOpusHeader(uint8_t* data, int len, int& ret_len) {
    int i = 0;

    if (len < 3) {
        return nullptr;
    }
    uint16_t prefix = ((uint16_t)data[0] << 8) | data[1];
    if (0x7fe0 != (prefix & 0xffe0)) {
        ret_len = len;
        return data;
    }

    int start_trim = (prefix >> 4) & 0x01;
    int end_trim   = (prefix >> 3) & 0x01;
    int ctrl_ext   = (prefix >> 2) & 0x01;
    int unit_size = data[2];

    for (i = 3; i < len && data[i-1] == 255; i++) {
        unit_size += data[i];
    }

    if (start_trim) {
        i += 2;
    }
    if (end_trim) {
        i += 2;
    }

    if (ctrl_ext) {
        i += 1 + data[i];
    }

    if (i + unit_size > len) {
        return nullptr;
    }
    ret_len = unit_size;
    return data + i;
}

/**
 * Read a 1- or 2-byte frame length
 */
static int xiph_lacing_16bit(uint8_t **ptr, const uint8_t *end)
{
    int val;

    if (*ptr >= end)
        return -1;
    val = *(*ptr)++;
    if (val >= 252) {
        if (*ptr >= end)
            return -1;
        val += 4 * *(*ptr)++;
    }
    return val;
}

/**
 * Read a multi-byte length (used for code 3 packet padding size)
 */
static int xiph_lacing_full(uint8_t **ptr, const uint8_t *end)
{
    int val = 0;
    int next;

    while (1) {
        if (*ptr >= end || val > INT_MAX - 254)
            return -1;
        next = *(*ptr)++;
        val += next;
        if (next < 255)
            break;
        else
            val--;
    }
    return val;
}

static bool GetOpusFrame(uint8_t* data, int len, std::vector<std::pair<uint8_t*, int>>& frames) {
    uint8_t* p = data;
    uint8_t* end = data + len;

    uint8_t type = *p;
    p++;
    len--;

    switch(type & 0x03) 
    {
        case 0://one frame
        {
            frames.push_back(std::make_pair(p - 1, len + 1));
            return true;
        }
        case 1://two cbr frames
        {
            if ((len % 2) == 1) {
                printf("two cbr frame len error:%d.\r\n", len);
                return false;
            }
            frames.push_back(std::make_pair(p - 1, len/2 + 1));
            p += len / 2;
            frames.push_back(std::make_pair(p - 1, len/2 + 1));
            p += len / 2;
            break;
        }
        case 2://2 frames, different sizes
        {
            int frame_len = xiph_lacing_16bit(&p, end);
            if (frame_len <= 0) {
                printf("two frames frame len error:%d.\r\n", frame_len);
                return false;
            }
            frames.push_back(std::make_pair(p - 1, frame_len + 1));
            p += frame_len;
            frame_len = end - p;
            frames.push_back(std::make_pair(p - 1, frame_len + 1));
            break;
        }
        case 3://1 to 48 frames, can be different sizes
        {
            int index = *p;
            p++;

            int frame_count = index & 0x3f;
            int padding     = (index >> 6) & 0x01;
            int vbr         = (index >> 7) & 0x01;

            if (frame_count <= 0 || frame_count > 48) {
                printf("3type frame count error:%d.\r\n", frame_count);
                return false;
            }

            //read padding size
            if (padding) {
                padding = xiph_lacing_full(&p, end);
                if (padding < 0) {
                    printf("padding error:%d.\r\n", padding);
                    return false;
                }
            }

            //read frame sizes
            if (vbr) {
                int total = 0;
                int frame_len = 0;
                std::vector<int> frame_size_vec;

                for(int i = 0; i < frame_count - 1; i++) {
                    frame_len = xiph_lacing_16bit(&p, end);
                    if (frame_len < 0) {
                        printf("vbr frame len:%d error.\r\n", frame_len);
                        return false;
                    }
                    frame_size_vec.push_back(frame_len);
                    total += frame_len;
                }

                frame_len = end - p - padding;
                if (total > frame_len) {
                    printf("vbr total(%d) > frame_len(%d)\r\n",
                            total, frame_len);
                    return false;
                }

                size_t size_count = frame_size_vec.size();
                for (int i = 0; i < (int)size_count; i++) {
                    if (i == (int)(size_count - 1)) {
                        int last_len = frame_len - total;
                        frames.push_back(std::make_pair(p - 1, last_len + 1));
                    } else {
                        frames.push_back(std::make_pair(p - 1, frame_size_vec[i] + 1));
                        p += frame_size_vec[i];
                    }
                }
            } else {//cbr
                int frame_len = end - p - padding;
                if (frame_len % frame_count
                    || (frame_len / frame_count > 48)) {
                    printf("cbr frame len:%d, frame count:%d.\r\n",
                            frame_len, frame_count);
                    return false;
                }
                frame_len /= frame_count;
                for (int i = 0; i < frame_count; i++) {
                    frames.push_back(std::make_pair(p - 1, frame_len + 1));
                    p += frame_len;
                }
            }
            break;
        }
    }
    return true;
}

bool GetOpusFrameVector(uint8_t* data, int len, std::vector<std::pair<uint8_t*, int>>& frames_vec) {
    uint8_t* p = data;
    uint8_t* end = p + len;
    int payload_len = 0;

    while(p < end) {
        p = GetOpusHeader(p, end - p, payload_len);
        if (!p) {
            printf("GetOpusHeader error.\r\n");
            return false;
        }
        
        bool ret = GetOpusFrame(p, payload_len, frames_vec);
        if (!ret) {
            printf("GetOpusFrame error.\r\n");
            return false;
        }
        p += payload_len;
    }
    return true;
}

bool GetOpusExtraData(int clock_rate, int channel, uint8_t* extra_data, size_t& extra_len) {
    uint8_t* p = extra_data;
    const char opus_str[] = "OpusHead";
    const size_t opus_str_len = 8;

    memcpy(p, (uint8_t*)opus_str, opus_str_len);
    p += opus_str_len;

    *p = 1;
    p++;
    *p = (uint8_t)channel;
    p++;
    ByteStream::Write2Bytes_le(p, 0);//initial_padding
    p += 2;
    ByteStream::Write4Bytes_le(p, clock_rate);
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
    extra_len = (size_t)(p - extra_data);
    return true;
    /*
       extra_len = sizeof(opus_default_extradata);
       memcpy(extra_data, opus_default_extradata, sizeof(opus_default_extradata));

       extra_data[9]  = channel;
       ByteStream::Write4Bytes_le(&extra_data[12], clock_rate);
       extra_data[18] = 0;
       extra_data[19] = opus_stream_cnt[channel];
       extra_data[20] = opus_coupled_stream_cnt[channel];
       memcpy(&extra_data[21], opus_channel_map[channel - 1], channel);
       extra_len = extra_data[18] ? 21 + channel : 19;
    return true;
       */
}

}
