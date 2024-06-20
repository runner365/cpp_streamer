/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2021 Runner365
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "mpegts_demux.hpp"
#include "h264_h265_header.hpp"
#include "opus_header.hpp"
#include "stringex.hpp"
#include "logger.hpp"
#include "uuid.hpp"

#include <string.h>
#include <stdio.h>

void* make_mpegtsdemux_streamer() {
    cpp_streamer::MpegtsDemux* demuxer = new cpp_streamer::MpegtsDemux();

    return demuxer;
}

void destroy_mpegtsdemux_streamer(void* streamer) {
    cpp_streamer::MpegtsDemux* demuxer = (cpp_streamer::MpegtsDemux*)streamer;

    delete demuxer;
}

namespace cpp_streamer
{
#define MPEGTS_DEMUX_NAME "mpegtsdemux"

std::map<std::string, std::string> MpegtsDemux::def_options_ = {
    {"re", "false"}
};

MpegtsDemux::MpegtsDemux():_data_total(0)
    ,_last_pid(0)
    ,_last_dts(0)
    ,_last_pts(0)
{
    name_ = MPEGTS_DEMUX_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
    options_ = def_options_;
}

MpegtsDemux::~MpegtsDemux() {

}

std::string MpegtsDemux::StreamerName() {
    return name_;
}

void MpegtsDemux::SetLogger(Logger* logger) {
    logger_ = logger;
}

int MpegtsDemux::AddSinker(CppStreamerInterface* sinker) {
    if (sinker == nullptr) {
        return 0;
    }
    sinkers_[sinker->StreamerName()] = sinker;
    return sinkers_.size();
}

int MpegtsDemux::RemoveSinker(const std::string& name) {
    return sinkers_.erase(name);
}

void MpegtsDemux::SetReporter(StreamerReport* reporter) {
    report_ = reporter;
}

void MpegtsDemux::ReportEvent(const std::string& type, const std::string& value) {
    if (report_) {
        report_->OnReport(name_, type, value);
    }
}

int MpegtsDemux::SourceData(Media_Packet_Ptr pkt_ptr) {
    return Decode(pkt_ptr->buffer_ptr_);
}

void MpegtsDemux::StartNetwork(const std::string& url, void* loop_handle) {

}

void MpegtsDemux::AddOption(const std::string& key, const std::string& value) {
    auto iter = options_.find(key);
    if (iter == options_.end()) {
        std::stringstream ss;
        ss << "the option key:" << key << " does not exist";
        throw CppStreamException(ss.str().c_str());
    }
    options_[key] = value;
    LogInfof(logger_, "set options key:%s, value:%s", key.c_str(), value.c_str());
}

int MpegtsDemux::DecodeUnit(unsigned char* data_p)
{
    int pos = 0;
    int npos = 0;
    TsHeader ts_header_info;

    ts_header_info._sync_byte = data_p[pos];
    pos++;

    ts_header_info._transport_error_indicator = (data_p[pos]&0x80)>>7;
    ts_header_info._payload_unit_start_indicator = (data_p[pos]&0x40)>>6;
    ts_header_info._transport_priority = (data_p[pos]&0x20)>>5;
    ts_header_info._PID = ((data_p[pos]<<8)|data_p[pos+1])&0x1FFF;
    pos += 2;
    
    ts_header_info._transport_scrambling_control = (data_p[pos]&0xC0)>>6;
    ts_header_info._adaptation_field_control = (data_p[pos]&0x30)>>4;
    ts_header_info._continuity_counter = (data_p[pos]&0x0F);
    pos++;
    npos = pos;

    // LogInfof(logger_, "ts header(0x%02x) payload_unit_start_indicator:%d, pid:%d, adaptation_field_control:%d, pos:%d, data[%d]:%d\r\n", 
    //     ts_header_info._sync_byte,
    //     ts_header_info._payload_unit_start_indicator, ts_header_info._PID,
    //     ts_header_info._adaptation_field_control, pos, pos, data_p[pos]);

    AdaptationField* field_p = &(ts_header_info._adaptation_field_info);
    // adaptation field
    // 0x01 No adaptation_field, payload only
    // 0x02 Adaptation_field only, no payload
    // 0x03 Adaptation_field followed by payload
    if( ts_header_info._adaptation_field_control == 2 
        || ts_header_info._adaptation_field_control == 3 ){
        // adaptation_field()
        field_p->_adaptation_field_length = data_p[pos];
        pos++;

        if( field_p->_adaptation_field_length > 0 ){
            field_p->_discontinuity_indicator = (data_p[pos]&0x80)>>7;
            field_p->_random_access_indicator = (data_p[pos]&0x40)>>6;
            field_p->_elementary_stream_priority_indicator = (data_p[pos]&0x20)>>5;
            field_p->_PCR_flag = (data_p[pos]&0x10)>>4;
            field_p->_OPCR_flag = (data_p[pos]&0x08)>>3;
            field_p->_splicing_point_flag = (data_p[pos]&0x04)>>2;
            field_p->_transport_private_data_flag = (data_p[pos]&0x02)>>1;
            field_p->_adaptation_field_extension_flag = (data_p[pos]&0x01);
            pos++;

            if( field_p->_PCR_flag == 1 ) { // PCR info
                //program_clock_reference_base 33 uimsbf
                //reserved 6 bslbf
                //program_clock_reference_extension 9 uimsbf
                pos += 6;
            }
            if( field_p->_OPCR_flag == 1 ) {
                //original_program_clock_reference_base 33 uimsbf
                //reserved 6 bslbf
                //original_program_clock_reference_extension 9 uimsbf
                pos += 6;
            }
            if( field_p->_splicing_point_flag == 1 ) {
                //splice_countdown 8 tcimsbf
                pos++;
            }
            if( field_p->_transport_private_data_flag == 1 ) {
                //transport_private_data_length 8 uimsbf
                field_p->_transport_private_data_length = data_p[pos];
                pos++;
                memcpy(field_p->_private_data_byte, data_p + pos, field_p->_transport_private_data_length);
            }
            if( field_p->_adaptation_field_extension_flag == 1 ) {
                //adaptation_field_extension_length 8 uimsbf
                field_p->_adaptation_field_extension_length = data_p[pos];
                pos++;
                //ltw_flag 1 bslbf
                field_p->_ltw_flag = (data_p[pos]&0x80)>>7;
                //piecewise_rate_flag 1 bslbf
                field_p->_piecewise_rate_flag = (data_p[pos]&0x40)>>6;
                //seamless_splice_flag 1 bslbf
                field_p->_seamless_splice_flag = (data_p[pos]&0x20)>>5;
                //reserved 5 bslbf
                pos++;
                if (field_p->_ltw_flag == 1) {
                    //ltw_valid_flag 1 bslbf
                    //ltw_offset 15 uimsbf
                    pos += 2;
                }
                if (field_p->_piecewise_rate_flag == 1) {
                    //reserved 2 bslbf
                    //piecewise_rate 22 uimsbf
                    pos += 3;
                }
                if (field_p->_seamless_splice_flag == 1) {
                    //splice_type 4 bslbf
                    //DTS_next_AU[32..30] 3 bslbf
                    //marker_bit 1 bslbf
                    //DTS_next_AU[29..15] 15 bslbf
                    //marker_bit 1 bslbf
                    //DTS_next_AU[14..0] 15 bslbf
                    //marker_bit 1 bslbf
                    pos += 5;
                }
            }
        }
        npos += sizeof(field_p->_adaptation_field_length) + field_p->_adaptation_field_length;
        pos = npos;
    }

    if(ts_header_info._adaptation_field_control == 1 
       || ts_header_info._adaptation_field_control == 3 ) {
        // data_byte with placeholder
        // payload parser
        if(ts_header_info._PID == 0x00){
            // PAT // program association table
            if(ts_header_info._payload_unit_start_indicator) {
                pos++;
            }
            _pat._table_id = data_p[pos];
            pos++;
            _pat._section_syntax_indicator = (data_p[pos]>>7)&0x01;
            // skip 3 bits of 1 zero and 2 reserved
            _pat._section_length = ((data_p[pos]<<8)|data_p[pos+1])&0x0FFF;
            pos += 2;
            _pat._transport_stream_id = (data_p[pos]<<8)|data_p[pos+1];
            pos += 2;
            // reserved 2 bits
            _pat._version_number = (data_p[pos]&0x3E)>>1;
            _pat._current_next_indicator = data_p[pos]&0x01;
            pos++;
            _pat._section_number = data_p[pos];
            pos++;
            _pat._last_section_number = data_p[pos];
            if (_pat._table_id != 0x00) {
                char error_sz[128];
                snprintf(error_sz, sizeof(error_sz), "pat tableid:0x%02x error, it must be 0x00", _pat._table_id);
                ReportEvent("error", error_sz);
                LogErrorf(logger_, "pat tableid:0x%02x error, it must be 0x00", _pat._table_id);
                return -1;
            }
            // PAT = section_length + 3
            if ((188 - npos) <= (_pat._section_length+3)) {
                char error_sz[128];
                snprintf(error_sz, sizeof(error_sz), "pat section length:0x%d error, the left length:%d", _pat._table_id, 188 - npos);
                ReportEvent("error", error_sz);
                LogErrorf(logger_, "pat section length:0x%d error, the left length:%d", _pat._table_id, 188 - npos);
                return -1;
            }
            pos++;
            _pat._pid_vec.clear();
            for (;pos+4 <= _pat._section_length-5-4+9 + npos;) { // 4:CRC, 5:follow section_length item  rpos + 4(following unit length) section_length + 9(above field and unit_start_first_byte )
                PID_INFO pid_info;
                //program_number 16 uimsbf
                pid_info._program_number = data_p[pos]<<8|data_p[pos+1];
                pos += 2;
//              reserved 3 bslbf
                
                if (pid_info._program_number == 0) {
//                  // network_PID 13 uimsbf
                    pid_info._network_id = (data_p[pos]<<8|data_p[pos+1])&0x1FFF;
                    //printf("#### network id:%d.\r\n", pid_info._network_id);
                    pos += 2;
                }
                else {
//                  //     program_map_PID 13 uimsbf
                    pid_info._pid = (data_p[pos]<<8|data_p[pos+1])&0x1FFF;
                    //printf("#### pmt id:%d.\r\n", pid_info._pid);
                    pos += 2;
                }
                _pat._pid_vec.push_back(pid_info);
                // network_PID and program_map_PID save to list
            }
//               CRC_32 use pat to calc crc32, eq
            pos += 4;
            reportPat();
        }else if(ts_header_info._PID == 0x01){
            // CAT // conditional access table
        }else if(ts_header_info._PID == 0x02){
            //TSDT  // transport stream description table
        }else if(ts_header_info._PID == 0x03){
            //IPMP // IPMP control information table
            // 0x0004-0x000F Reserved
            // 0x0010-0x1FFE May be assigned as network_PID, Program_map_PID, elementary_PID, or for other purposes
        }else if(ts_header_info._PID == 0x11){
            // SDT // https://en.wikipedia.org/wiki/Service_Description_Table / https://en.wikipedia.org/wiki/MPEG_transport_stream
        }else if(IsPmt(ts_header_info._PID)) {
            if(ts_header_info._payload_unit_start_indicator)
                pos++;
            _pmt._table_id = data_p[pos];
            pos++;
            _pmt._section_syntax_indicator = (data_p[pos]>>7)&0x01;
            // skip 3 bits of 1 zero and 2 reserved
            _pmt._section_length = ((data_p[pos]<<8)|data_p[pos+1])&0x0FFF;
            pos += 2;
            _pmt._program_number = (data_p[pos]<<8)|data_p[pos+1];
            pos += 2;
            // reserved 2 bits
            _pmt._version_number = (data_p[pos]&0x3E)>>1;
            _pmt._current_next_indicator = data_p[pos]&0x01;
            pos++;
            _pmt._section_number = data_p[pos];
            pos++;
            _pmt._last_section_number = data_p[pos];
            pos++;
            // skip 3 bits for reserved 3 bslbf
            _pmt._PCR_PID = ((data_p[pos]<<8)|data_p[pos+1])&0x1FFF; //PCR_PID 13 uimsbf
            pos += 2;

            //reserved 4 bslbf
            _pmt._program_info_length = ((data_p[pos]<<8)|data_p[pos+1])&0x0FFF;//program_info_length 12 uimsbf
            pos += 2;
            //  0x02, // TS_program_map_section
            if (_pmt._table_id != 0x02) {
                char error_sz[128];
                snprintf(error_sz, sizeof(error_sz), "pmt tableid(0x%02x) error, it must be 0x02", _pmt._table_id);
                ReportEvent("error", error_sz);
                LogErrorf(logger_, "pmt tableid(0x%02x) error, it must be 0x02", _pmt._table_id);
                return -1;
            }

            memcpy(_pmt._dscr, data_p+pos, _pmt._program_info_length);
//               for (i = 0; i < N; i++) {
//                   descriptor()
//               }
            pos += _pmt._program_info_length;
            _pmt._stream_pid_vec.clear();
            _pmt._pid2steamtype.clear();

            for (; pos + 5 <= _pmt._section_length + 4 - 4 + npos; ) { // pos(above field length) i+5(following unit length) section_length +3(PMT begin three bytes)+1(payload_unit_start_indicator) -4(crc32)
                STREAM_PID_INFO pid_info;
                pid_info._stream_type = data_p[pos];//stream_type 8 uimsbf  0x1B AVC video stream as defined in ITU-T Rec. H.264 | ISO/IEC 14496-10 Video
                pos++;
                //reserved 3 bslbf
                pid_info._elementary_PID = ((data_p[pos]<<8)|data_p[pos+1])&0x1FFF; //elementary_PID 13 uimsbf
                pos += 2;
                //reserved 4 bslbf
                pid_info._ES_info_length = ((data_p[pos]<<8)|data_p[pos+1])&0x0FFF; //ES_info_length 12 uimsbf
                pos += 2;
                if( pos + pid_info._ES_info_length > _pmt._section_length + 4 - 4 + npos )
                    break;
                int absES_info_length = pos + pid_info._ES_info_length;
                for (; pos< absES_info_length; ) {
                    char desc[4096];
                    ES_INFO info;

                    //descriptor()
                    int descriptor_tag = data_p[pos];
                    pos++;
                    int descriptor_length = data_p[pos];
                    pos++;
                    memcpy(desc, data_p + pos, descriptor_length);
                    desc[descriptor_length] = 0;

                    info.desc_ = desc;
                    info.descriptor_tag_ = descriptor_tag;

                    LogDebugf(logger_, "descriptor_tag:0x%02x, es info:%s, pid:%d", descriptor_tag, desc, pid_info._elementary_PID);
                    pos += descriptor_length;
                    pid_info.es_info_vec.push_back(info);
                }
                
                // save program_number(stream num) elementary_PID(PES PID) stream_type(stream codec)
                //printf("pmt pid:%d, streamtype:%d, pos:%d\r\n", pid_info._elementary_PID, pid_info._stream_type, pos);
                _pmt._stream_pid_vec.push_back(pid_info);
                _pmt._pid2steamtype.insert(std::make_pair((unsigned short)pid_info._elementary_PID, pid_info._stream_type));
            }
            pos += 4;//CRC_32
            reportPmt();
        }else if(ts_header_info._PID == 0x0042){
            // USER
        }else if(ts_header_info._PID == 0x1FFF){
            // Null packet
        }else{//pes packet or pure data packet
            //bool isFound = false;
            for (size_t i = 0; i < _pmt._stream_pid_vec.size(); i++) {
                if(ts_header_info._PID == _pmt._stream_pid_vec[i]._elementary_PID){
                    //isFound = true;
                    if(ts_header_info._payload_unit_start_indicator){
                        unsigned char* ret_data_p = nullptr;
                        size_t ret_size = 0;
                        uint64_t dts = 0;
                        uint64_t pts = 0;

                        //callback last media data in data buffer
                        OnCallback(_last_pid, _last_dts, _last_pts);

                        int ret = PesParse(data_p+npos, npos, &ret_data_p, ret_size, dts, pts);
                        if (ret > 188) {
                            char error_sz[128];
                            snprintf(error_sz, sizeof(error_sz), "pes length(%d) error", ret);
                            ReportEvent("error", error_sz);
                            LogErrorf(logger_, "pes length(%d) error", ret);
                            return -1;
                        }

                        _last_pts = pts;
                        _last_dts = (dts == 0) ? pts : dts;

                        if ((ret_data_p != nullptr) && (ret_size > 0)) {
                            InsertIntoDatabuf(ret_data_p, ret_size, ts_header_info._PID);
                        }
                    }else{
                        //fwrite(p, 1, 188-(npos+pos), pes_info[i].fd);
                        InsertIntoDatabuf(data_p + npos, 188-npos, ts_header_info._PID);
                    }
                }
            }
            //if(!isFound){
            //    printf("unknown PID = %X \n", ts_header_info._PID);
            //}
        }
    }

    return 0;
}

int MpegtsDemux::Decode(DATA_BUFFER_PTR data_ptr)
{
    int ret = -1;
    std::string path;

    if (!data_ptr || (data_ptr->DataLen() < 188) || (data_ptr->DataLen()%188 != 0))
    {
        char error_sz[128];
        snprintf(error_sz, sizeof(error_sz), "mpegts len must be divided d evenly by 188");
        ReportEvent("error", error_sz);
        LogErrorf(logger_, "input data error, data len:%d", data_ptr->DataLen());
        return -1;
    }

    unsigned int count = data_ptr->DataLen()/188;
    for (unsigned int index = 0; index < count; index++)
    {
        unsigned char* data = (uint8_t*)data_ptr->Data() + 188*index;
        if (data[0] != 0x47) {
            continue;
        }
        ret = DecodeUnit(data);
        if (ret < 0)
        {
            break;
        }
    }
    return ret;
}

void MpegtsDemux::InsertIntoDatabuf(unsigned char* data_p, size_t data_size, unsigned short pid) {
    _last_pid = pid;
    _data_total += data_size;

    DATA_BUFFER_PTR data_ptr = std::make_shared<DataBuffer>();
    data_ptr->AppendData((char*)data_p, data_size);
    _data_buffer_vec.push_back(data_ptr);
    return;
}

int MpegtsDemux::GetMediaInfoByPid(uint16_t pid, MEDIA_PKT_TYPE& media_type, MEDIA_CODEC_TYPE& codec_type) {
    media_type = MEDIA_UNKOWN_TYPE;
    codec_type = MEDIA_CODEC_UNKOWN;

    auto iter = _pmt._pid2steamtype.find(pid);
    if (iter != _pmt._pid2steamtype.end()) {
        unsigned char stream_type = iter->second;

        GetMediaInfoByStreamtype(stream_type, media_type, codec_type);
        if ((media_type != MEDIA_UNKOWN_TYPE) && (codec_type != MEDIA_CODEC_UNKOWN)) {
            return 0;
        }
    }

    for (STREAM_PID_INFO& info : _pmt._stream_pid_vec) {
        for (ES_INFO& es_info : info.es_info_vec) {
            std::string desc(es_info.desc_);

            String2Lower(desc);
            if ((info._elementary_PID == pid) && (desc == "opus")) {
                media_type = MEDIA_AUDIO_TYPE;
                codec_type = MEDIA_CODEC_OPUS;
            }
        
        }
   }
    return 0;
}


void MpegtsDemux::OnCallback(unsigned short pid, uint64_t dts, uint64_t pts) {
    if ((_data_total <=0 ) || (_data_buffer_vec.empty())) {
        return;
    }
    MEDIA_PKT_TYPE media_type = MEDIA_UNKOWN_TYPE;
    MEDIA_CODEC_TYPE codec_type = MEDIA_CODEC_UNKOWN;

    GetMediaInfoByPid(pid, media_type, codec_type);

    Media_Packet_Ptr pkt_ptr = std::make_shared<Media_Packet>();

    pkt_ptr->av_type_    = media_type;
    pkt_ptr->codec_type_ = codec_type;
    pkt_ptr->fmt_type_   = MEDIA_FORMAT_RAW;
    pkt_ptr->dts_        = dts/90;
    pkt_ptr->pts_        = pts/90;

    for (size_t index = 0; index < _data_buffer_vec.size(); index++) {
       uint8_t* p = (uint8_t*)_data_buffer_vec[index]->Data();
       int len = _data_buffer_vec[index]->DataLen();

       if (len > 0) {
           pkt_ptr->buffer_ptr_->AppendData((char*)p, len);
       }
    }
    _data_buffer_vec.clear();
    _data_total = 0;

    if (media_type == MEDIA_UNKOWN_TYPE) {
        LogInfof(logger_, "pid:%d, buffer len:%lu, data:%s", pid, pkt_ptr->buffer_ptr_->DataLen(),
            std::string(pkt_ptr->buffer_ptr_->Data(), pkt_ptr->buffer_ptr_->DataLen()).c_str());
    }
    if (media_type == MEDIA_VIDEO_TYPE) {
        std::vector<std::shared_ptr<DataBuffer>> databuffers;
        bool ret = AnnexB2Nalus((uint8_t*)pkt_ptr->buffer_ptr_->Data(),
                pkt_ptr->buffer_ptr_->DataLen(), databuffers);
        if (ret) {
            for (auto& data_buffer : databuffers) {
                uint8_t* p = (uint8_t*)data_buffer->Data();
                int nalu_len = data_buffer->DataLen();
                int nalu_type_pos = -1;

                if (nalu_len < 5) {
                    continue;
                }

                nalu_type_pos = GetNaluTypePos(p);

                if (H264_IS_AUD(p[nalu_type_pos])) {
                    continue;
                }
                Media_Packet_Ptr output_ptr = std::make_shared<Media_Packet>();
                output_ptr->copy_properties(*(pkt_ptr.get()));
                output_ptr->buffer_ptr_->Reset();

                output_ptr->is_seq_hdr_   = H264_IS_SPS(p[nalu_type_pos]) || H264_IS_PPS(p[nalu_type_pos]);
                output_ptr->is_key_frame_ = H264_IS_KEYFRAME(p[nalu_type_pos]);

                output_ptr->buffer_ptr_->AppendData((char*)p, nalu_len);
                Output(output_ptr);
            }
        }
        return;
    }

    if (media_type == MEDIA_AUDIO_TYPE) {
        if (codec_type == MEDIA_CODEC_OPUS) {
            uint8_t* data = (uint8_t*)pkt_ptr->buffer_ptr_->Data();
            int total =  (int)pkt_ptr->buffer_ptr_->DataLen();
            int64_t dts = pkt_ptr->dts_;
            std::vector<std::pair<uint8_t*, int>>  data_vec;

            if (opus_dts_ < 0) {
                opus_dts_ = pkt_ptr->dts_;
            }

            //LogInfoData(logger_, data, total, "opus raw data");
            bool ret = GetOpusFrameVector(data, total, data_vec);
            if (!ret) {
                LogErrorf(logger_, "GetOpusFrameVector error");
                return;
            }
            opus_count_++;

            for (size_t i = 0; i < data_vec.size(); i++) {
                uint8_t* data_p = data_vec[i].first;
                size_t len = data_vec[i].second;

                //LogInfof(logger_, "opus data index:%lu, data_p:%p, len:%lu",
                //        i, data_p, len);

                Media_Packet_Ptr new_pkt_ptr = std::make_shared<Media_Packet>();
                new_pkt_ptr->copy_properties(pkt_ptr);
                new_pkt_ptr->buffer_ptr_->Reset();
                new_pkt_ptr->buffer_ptr_->AppendData((char*)data_p, len);
                new_pkt_ptr->dts_ = dts + 20 * i;
                new_pkt_ptr->pts_ = dts + 20 * i;
                opus_dts_ += 20;
                Output(new_pkt_ptr);
            }
            return;
        }
    }

    Output(pkt_ptr);
    return;
}

void MpegtsDemux::Output(Media_Packet_Ptr pkt_ptr) {
    if (options_["re"] == "true") {
        waiter_.Wait(pkt_ptr);
    }

    /*
    if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        LogInfof(logger_, "audio packet len:%lu", pkt_ptr->buffer_ptr_->DataLen());
        LogInfoData(logger_, (uint8_t*)pkt_ptr->buffer_ptr_->Data(),
                pkt_ptr->buffer_ptr_->DataLen(),
                "media audio");
    }
    */

    for(auto& sinker : sinkers_) {
        sinker.second->SourceData(pkt_ptr);
    }
}

bool MpegtsDemux::IsPmt(unsigned short pid) {
    for (size_t index = 0; index < _pat._pid_vec.size(); index++) {
        if (_pat._pid_vec[index]._program_number != 0) {
            if (_pat._pid_vec[index]._pid == pid) {
                return true;
            }
        }
    }
    return false;
}


int MpegtsDemux::PesParse(unsigned char* p, size_t npos, 
                        unsigned char** ret_pp, size_t& ret_size,
                        uint64_t& dts, uint64_t& pts) {
    int pos = 0;
    int packet_start_code_prefix = (p[pos]<<16)|(p[pos+1]<<8)|p[pos+2];  //packet_start_code_prefix 24 bslbf
    pos += 3;
    int stream_id = p[pos]; //stream_id 8 uimsbf
    pos++;
    //printf("pes parse %02x %02x.\r\n", p[pos], p[pos+1]);
    int PES_packet_length = ((unsigned int)p[pos]<<8)|p[pos+1]; //PES_packet_length 16 uimsbf
    (void)PES_packet_length;
    pos += 2;
    //printf("pes parse packet_start_code_prefix:%d, npos:%lu, PES_packet_length:%d, stream_id:%d.\r\n", 
    //    packet_start_code_prefix, npos, PES_packet_length, stream_id);
    if (0x00000001 != packet_start_code_prefix) {
        char error_sz[128];
        snprintf(error_sz, sizeof(error_sz), "packet_start_code_prefix:0x%08x error, and streamid:%d, pes packet length:%d",
            packet_start_code_prefix, stream_id, PES_packet_length);
        ReportEvent("error", error_sz);
        LogErrorf(logger_, "packet_start_code_prefix:0x%08x error, and streamid:%d, pes packet length:%d",
            packet_start_code_prefix, stream_id, PES_packet_length);
        return 255;
    }

    if (stream_id != 188//program_stream_map 1011 1100
        && stream_id != 190//padding_stream 1011 1110
        && stream_id != 191//private_stream_2 1011 1111
        && stream_id != 240//ECM 1111 0000
        && stream_id != 241//EMM 1111 0001
        && stream_id != 255//program_stream_directory 1111 1111
        && stream_id != 242//DSMCC_stream 1111 0010
        && stream_id != 248//ITU-T Rec. H.222.1 type E stream 1111 1000
        ) 
    {
        if (0x80 != (p[pos] & 0xc0)) {
            char error_sz[128];
            snprintf(error_sz, sizeof(error_sz), "the first 2 bits:0x%02x error, it must be 0x80.", (p[pos] & 0xc0));
            ReportEvent("error", error_sz);
            LogErrorf(logger_, "the first 2 bits:0x%02x error, it must be 0x80.", (p[pos] & 0xc0));
            return 255;
        }
        //skip 2bits//'10' 2 bslbf
        int PES_scrambling_control = (p[pos]&30)>>4; //PES_scrambling_control 2 bslbf
        (void)PES_scrambling_control;
        int PES_priority = (p[pos]&0x08)>>3; //PES_priority 1 bslbf
        (void)PES_priority;
        int data_alignment_indicator = (p[pos]&0x04)>>2;//data_alignment_indicator 1 bslbf
        (void)data_alignment_indicator;
        int copyright = (p[pos]&0x02)>>1; //copyright 1 bslbf
        (void)copyright;
        int original_or_copy = (p[pos]&0x01);//original_or_copy 1 bslbf
        (void)original_or_copy;
        pos++;
        int PTS_DTS_flags = (p[pos]&0xC0)>>6; //PTS_DTS_flags 2 bslbf
        int ESCR_flag = (p[pos]&0x20)>>5; // ESCR_flag 1 bslbf
        int ES_rate_flag = (p[pos]&0x10)>>4;//ES_rate_flag 1 bslbf
        int DSM_trick_mode_flag = (p[pos]&0x08)>>3;//DSM_trick_mode_flag 1 bslbf
        int additional_copy_info_flag = (p[pos]&0x04)>>2; //additional_copy_info_flag 1 bslbf
        int PES_CRC_flag = (p[pos]&0x02)>>1; //PES_CRC_flag 1 bslbf
        int PES_extension_flag = (p[pos]&0x01);//PES_extension_flag 1 bslbf
        pos++;
        int PES_header_data_length = p[pos]; //PES_header_data_length 8 uimsbf
        (void)PES_header_data_length;
        pos++;

        if (PTS_DTS_flags == 2) {
            // skip 4 bits '0010' 4 bslbf
            // PTS [32..30] 3 bslbf
            // marker_bit 1 bslbf
            // PTS [29..15] 15 bslbf
            // marker_bit 1 bslbf
            // PTS [14..0] 15 bslbf
            // marker_bit 1 bslbf
            pts = (((p[pos]>>1)&0x07) << 30) | (p[pos+1]<<22) | (((p[pos+2]>>1)&0x7F)<<15) | (p[pos+3]<<7) | ((p[pos+4]>>1)&0x7F);
            pos += 5;
        }
        if (PTS_DTS_flags == 3) {
            // '0011' 4 bslbf
            // PTS [32..30] 3 bslbf
            // marker_bit 1 bslbf
            //PTS [29..15] 15 bslbf
            //marker_bit 1 bslbf
            // PTS [14..0] 15 bslbf
            // marker_bit 1 bslbf
            pts = (((p[pos]>>1)&0x07) << 30) | (p[pos+1]<<22) | (((p[pos+2]>>1)&0x7F)<<15) | (p[pos+3]<<7) | ((p[pos+4]>>1)&0x7F);
            pos += 5;
            // '0001' 4 bslbf
            // DTS [32..30] 3 bslbf
            // marker_bit 1 bslbf
            // DTS [29..15] 15 bslbf
            // marker_bit 1 bslbf
            // DTS [14..0] 15 bslbf
            // marker_bit 1 bslbf
            dts = (((p[pos]>>1)&0x07) << 30) | (p[pos+1]<<22) | (((p[pos+2]>>1)&0x7F)<<15) | (p[pos+3]<<7) | ((p[pos+4]>>1)&0x7F);
            pos += 5;
        }
        if (ESCR_flag == 1) {
            // reserved 2 bslbf
            // ESCR_base[32..30] 3 bslbf
            // marker_bit 1 bslbf
            // ESCR_base[29..15] 15 bslbf
            // marker_bit 1 bslbf
            // ESCR_base[14..0] 15 bslbf
            // marker_bit 1 bslbf
            // ESCR_extension 9 uimsbf
            // marker_bit 1 bslbf
            uint64_t ESCR_base = ((((uint64_t)p[pos] >> 3) & 0x07) << 30) | (((uint64_t)p[pos] & 0x03) << 28) | ((uint64_t)p[pos + 1] << 20) | ((((uint64_t)p[pos + 2] >> 3) & 0x1F) << 15) | (((uint64_t)p[pos + 2] & 0x3) << 13) | ((uint64_t)p[pos + 3] << 5) | ((p[pos + 4] >> 3) & 0x1F);
            int ESCR_extension = ((p[pos + 4] & 0x03) << 7) | ((p[pos + 5] >> 1) & 0x7F);
            (void)ESCR_base;
            (void)ESCR_extension;
            pos += 6;
        }
        if (ES_rate_flag == 1) {
            // marker_bit 1 bslbf
            // ES_rate 22 uimsbf
            // marker_bit 1 bslbf
            int ES_rate = (p[pos]&0x7F)<<15 | (p[pos+1])<<7 | (p[pos+2]&0x7F)>>1;
            (void)ES_rate;
            pos += 3;
        }
        if (DSM_trick_mode_flag == 1) { // ignore
            int trick_mode_control = (p[pos]&0xE0)>>5;//trick_mode_control 3 uimsbf
            if ( trick_mode_control == 0/*fast_forward*/ ) {
                // field_id 2 bslbf
                // intra_slice_refresh 1 bslbf
                // frequency_truncation 2 bslbf
            }
            else if ( trick_mode_control == 1/*slow_motion*/ ) {
                //rep_cntrl 5 uimsbf
            }
            else if ( trick_mode_control == 2/*freeze_frame*/ ) {
                // field_id 2 uimsbf
                // reserved 3 bslbf
            }
            else if ( trick_mode_control == 3/*fast_reverse*/ ) {
                // field_id 2 bslbf
                // intra_slice_refresh 1 bslbf
                // frequency_truncation 2 bslbf
            }else if ( trick_mode_control == 4/*slow_reverse*/ ) {
                // rep_cntrl 5 uimsbf
            }
            else{
                //reserved 5 bslbf
            }
            pos++;
        }
        if ( additional_copy_info_flag == 1) { // ignore
            // marker_bit 1 bslbf
            // additional_copy_info 7 bslbf
            pos++;
        }
        if ( PES_CRC_flag == 1) { // ignore
            // previous_PES_packet_CRC 16 bslbf
            pos += 2;
        }
        if ( PES_extension_flag == 1) { // ignore
            int PES_private_data_flag = (p[pos]&0x80)>>7;// PES_private_data_flag 1 bslbf
            int pack_header_field_flag = (p[pos]&0x40)>>6;// pack_header_field_flag 1 bslbf
            int program_packet_sequence_counter_flag = (p[pos]&0x20)>>5;// program_packet_sequence_counter_flag 1 bslbf
            int P_STD_buffer_flag = (p[pos]&0x10)>>4; // P-STD_buffer_flag 1 bslbf
            // reserved 3 bslbf
            int PES_extension_flag_2 = (p[pos]&0x01);// PES_extension_flag_2 1 bslbf
            pos++;

            if ( PES_private_data_flag == 1) {
                // PES_private_data 128 bslbf
                pos += 16;
            }
            if (pack_header_field_flag == 1) {
                // pack_field_length 8 uimsbf
                // pack_header()
            }
            if (program_packet_sequence_counter_flag == 1) {
                // marker_bit 1 bslbf
                // program_packet_sequence_counter 7 uimsbf
                // marker_bit 1 bslbf
                // MPEG1_MPEG2_identifier 1 bslbf
                // original_stuff_length 6 uimsbf
                pos += 2;
            }
            if ( P_STD_buffer_flag == 1) {
                // '01' 2 bslbf
                // P-STD_buffer_scale 1 bslbf
                // P-STD_buffer_size 13 uimsbf
                pos += 2;
            }
            if ( PES_extension_flag_2 == 1) {
                // marker_bit 1 bslbf
                int PES_extension_field_length = (p[pos]&0x7F);// PES_extension_field_length 7 uimsbf
                pos++;
                for (int i = 0; i < PES_extension_field_length; i++) {
                    // reserved 8 bslbf
                    pos++;
                }
            }
        }

//        for (int i = 0; i < N1; i++) {
        //stuffing_byte 8 bslbf
//            rpos++;
//        }
//        for (int i = 0; i < N2; i++) {
        //PES_packet_data_byte 8 bslbf
//        rpos++;
//        }
        *ret_pp = p+pos;
        ret_size = 188-(npos+pos);
        //printf("pes parse body size:%lu, data:0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x, dts:%lu(%lu), pts:%lu(%lu)\r\n",
        //    ret_size, p[pos], p[pos+1], p[pos+2], p[pos+3], p[pos+4], p[pos+5], 
        //    dts, dts/90, pts, pts/90);
    }
    else if ( stream_id == 188//program_stream_map 1011 1100 BC
             || stream_id == 191//private_stream_2 1011 1111 BF
             || stream_id == 240//ECM 1111 0000 F0
             || stream_id == 241//EMM 1111 0001 F1
             || stream_id == 255//program_stream_directory 1111 1111 FF
             || stream_id == 242//DSMCC_stream 1111 0010 F2
             || stream_id == 248//ITU-T Rec. H.222.1 type E stream 1111 1000 F8
             ) {
//        for (i = 0; i < PES_packet_length; i++) {
         //PES_packet_data_byte 8 bslbf
//         rpos++;
//        }
        *ret_pp = p+pos;
        ret_size = 188-(npos+pos);
        //fwrite(p, 1, 188-(npos+rpos), fd);
    }
    else if ( stream_id == 190//padding_stream 1011 1110
             ) {
//        for (i = 0; i < PES_packet_length; i++) {
        // padding_byte 8 bslbf
//            rpos++;
        *ret_pp = p+pos;
        ret_size = 188-(npos+pos);
//        }
    }
    
    return pos;
}

void MpegtsDemux::reportPat() {
    std::string rpt = _pat.Dump();

    ReportEvent("pat", rpt);
}

void MpegtsDemux::reportPmt() {
    std::string rpt = _pmt.Dump();

    ReportEvent("pmt", rpt);
}

}

