#include "mp4_demux.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "uuid.hpp"
#include "media_packet.hpp"
#include "audio_header.hpp"
#include "h264_h265_header.hpp"
#include "stringex.hpp"

#include <stdio.h>

#define MP4_DEMUX_NAME "mp4demux"

void* make_mp4demux_streamer() {
    cpp_streamer::Mp4Demuxer* demuxer = new cpp_streamer::Mp4Demuxer();

    return demuxer;
}

void destroy_mp4demux_streamer(void* streamer) {
    cpp_streamer::Mp4Demuxer* demuxer = (cpp_streamer::Mp4Demuxer*)streamer;

    delete demuxer;
}

namespace cpp_streamer
{
std::map<std::string, std::string> Mp4Demuxer::def_options_;

Mp4Demuxer::Mp4Demuxer()
{
    name_ = MP4_DEMUX_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
    options_ = def_options_;
}

Mp4Demuxer::~Mp4Demuxer()
{

}

std::string Mp4Demuxer::StreamerName() {
    return name_;
}

int Mp4Demuxer::AddSinker(CppStreamerInterface* sinker) {

    return 0;
}

int Mp4Demuxer::RemoveSinker(const std::string& name) {
    return 0;
}

int Mp4Demuxer::SourceData(Media_Packet_Ptr pkt_ptr) {
    buffer_.AppendData(pkt_ptr->buffer_ptr_->Data(), pkt_ptr->buffer_ptr_->DataLen());

    uint8_t* p = (uint8_t*)buffer_.Data();
    while (p < (uint8_t*)(buffer_.Data() + buffer_.DataLen())) {
        std::string box_type;
        int offset = 0;
        uint64_t box_size = GetBoxHeaderInfo(p, box_type, offset);

        if ((p + box_size) > (uint8_t*)(buffer_.Data() + buffer_.DataLen())) {
            break;
        }
        (void)offset;

        if (box_type == "ftyp") {
            ftyp_box_ = new FtypBox();
            p = ftyp_box_->Parse(p);
            LogInfof(logger_, "%s", ftyp_box_->Dump().c_str());
        } else if (box_type == "moov") {
            moov_box_ = new MoovBox();
            p = moov_box_->Parse(p);
            std::string moov_dump = moov_box_->Dump();
            std::cout << "moov dump:" << moov_dump << "\r\n\r\n";
        } else if (box_type == "free") {
            free_box_ = new FreeBox();
            p = free_box_->Parse(p);
            LogInfof(logger_, "%s", free_box_->Dump().c_str());
        } else if (box_type == "mdat") {
            mdat_box_ = new MdatBox();
            p = mdat_box_->Parse(p);
            LogInfof(logger_, "%s", mdat_box_->Dump().c_str());
        } else {
            LogWarnf(logger_, "unknown box, box_type:%s", box_type.c_str());
            assert(0);
        }
    }
    buffer_.ConsumeData(p - (uint8_t*)buffer_.Data());
    return 0;
}

void Mp4Demuxer::AddOption(const std::string& key, const std::string& value) {

}

void Mp4Demuxer::SetReporter(StreamerReport* reporter) {

}

}