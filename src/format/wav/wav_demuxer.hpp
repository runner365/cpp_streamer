#ifndef WAV_DEC_HPP
#define WAV_DEC_HPP
#include "utils/io_interface.hpp"
#include "utils/logger.hpp"
#include "wav_pub.hpp"
#include <stdint.h>
#include <stddef.h>

namespace cpp_streamer
{

class WavDemuxer
{
private:
    IoReaderI* io_reader_   = nullptr;
    WavDataCallbackI* cb_   = nullptr;
    FormatChunk format_;
    Logger* logger_         = nullptr;

public:
    WavDemuxer(IoReaderI* io_reader, WavDataCallbackI* cb, Logger* logger);
    ~WavDemuxer();

public:
    int Demux();

private:
    int ReadChunkFmt();
    int ReadChunkData();
    int ReadChunkList(uint32_t chunk_size);
};

}
#endif