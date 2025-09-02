#include "wav_demuxer.hpp"
#include <string.h>

namespace cpp_streamer
{

WavDemuxer::WavDemuxer(IoReaderI* io_reader, WavDataCallbackI* cb, Logger* logger):io_reader_(io_reader)
                                                                                , cb_(cb)
                                                                                , logger_(logger)
{
}

WavDemuxer::~WavDemuxer()
{
}

int WavDemuxer::ReadChunkFmt() {
    uint8_t* p = (uint8_t*)&format_;

    int ret = io_reader_->IoRead(p, sizeof(FormatChunk));
    if (ret < sizeof(FormatChunk)) {
        LogErrorf(logger_, "read wav format chunk error");
        return -1;
    }
    std::string dump = WavFormatChunkDump(&format_);
    LogInfof(logger_, "wav format chunk:%s", dump.c_str());
    return sizeof(FormatChunk);
}

int WavDemuxer::ReadChunkData() {
    int ret = -1;
    size_t read_len = format_.numChannels * (format_.bitsPerSample / 8) * format_.sampleRate;
    read_len = read_len / 50;

    LogInfof(logger_, "wav read sample len:%lu", read_len);
    std::vector<uint8_t> data_buffer(read_len);
    uint8_t* data_ptr = data_buffer.data();

    while (true) {
        ret = io_reader_->IoRead(data_ptr, read_len);
        if (ret < read_len) {
            LogInfof(logger_, "read wav data eof");
            break;
        }
        if (cb_) {
            cb_->OnWavPacketCallBack(data_ptr, read_len, &format_);
        }
    }
    return 0;
}

int WavDemuxer::ReadChunkList(uint32_t chunk_size) {
    std::vector<uint8_t> data(chunk_size);
    uint8_t* p = (uint8_t*)&data[0];

    int ret = io_reader_->IoRead(p, chunk_size);
    if (ret < chunk_size) {
        LogErrorf(logger_, "read wav list chunk error");
        return -1;
    }
    LogInfof(logger_, "wav list chunk size:%u", chunk_size);

    return 0;
}

int WavDemuxer::Demux() {
    RIFFHeader riff_header;
    int ret = -1;

    uint8_t* p = (uint8_t*)&riff_header;
    ret = io_reader_->IoRead(p, sizeof(RIFFHeader));
    if (ret < sizeof(RIFFHeader)) {
        LogErrorf(logger_, "read wav header error");
        return -1;
    }
    if (strncmp(riff_header.chunkId, "RIFF", 4) != 0 || strncmp(riff_header.format, "WAVE", 4) != 0) {
        LogErrorf(logger_, "wav header error");
        return -1;
    }
    LogInfof(logger_, "wav header get chunk size:%u", riff_header.chunkSize);

    // try to read chunk header
    while (true) {
        ChunkHeader chunk_header;
        p = (uint8_t*)&chunk_header;
        ret = io_reader_->IoRead(p, sizeof(ChunkHeader));
        if (ret < sizeof(ChunkHeader)) {
            LogErrorf(logger_, "read wav chunk header error");
            return -1;
        }
        if (strncmp(chunk_header.subchunkId, "fmt ", 4) == 0) {
            ret = ReadChunkFmt();
            if (ret < 0) {
                LogErrorf(logger_, "read wav chunk fmt error");
                return -1;
            }
            continue;
        }
        if (strncmp(chunk_header.subchunkId, "LIST", 4) == 0) {
            ret = ReadChunkList(chunk_header.subchunkSize);
            if (ret < 0) {
                LogErrorf(logger_, "read wav chunk list error");
                return -1;
            }
            continue;
        }
        if (strncmp(chunk_header.subchunkId, "data", 4) == 0) {
            LogInfof(logger_, "wav data chunk size:%u", chunk_header.subchunkSize);
            return ReadChunkData();
        }
        LogInfof(logger_, "unhandle wav chunk type:%c%c%c%c",
            chunk_header.subchunkId[0], chunk_header.subchunkId[1],
            chunk_header.subchunkId[2], chunk_header.subchunkId[3]);

        std::vector<uint8_t> temp_data(chunk_header.subchunkSize);  
        p = (uint8_t*)&temp_data[0];
        ret = io_reader_->IoRead(p, chunk_header.subchunkSize);
        if (ret < chunk_header.subchunkSize) {
            LogErrorf(logger_, "read wav chunk data error");
            return -1;
        }
    }
    
    return 0;
}
}