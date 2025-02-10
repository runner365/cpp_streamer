#ifndef OGG_DEMUXER_HPP
#define OGG_DEMUXER_HPP
#include "ogg_pub.hpp"
#include "utils/io_interface.hpp"
#include "utils/logger.hpp"
#include "format/opus_header.hpp"
#include <vector>

namespace cpp_streamer
{

class OggDemuxer
{
private:
    IoReaderI* io_reader_   = nullptr;
    OpusDataCallbackI* cb_  = nullptr;
    Logger* logger_         = nullptr;

private:
    OpusExtraHandler extra_data_handler_;
    int64_t dts_ = 0;
    int64_t dts_interval_ = 20;
    
public:
    OggDemuxer(IoReaderI* io_reader, OpusDataCallbackI* cb, Logger* logger);
    ~OggDemuxer();

public:
    int Demux();
    void SetDtsInterval(int64_t dts_interval);
};

}
#endif