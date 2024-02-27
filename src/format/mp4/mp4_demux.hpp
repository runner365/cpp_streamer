#ifndef MP4_DEMUX_HPP
#define MP4_DEMUX_HPP
#include "data_buffer.hpp"
#include "cpp_streamer_interface.hpp"
#include "logger.hpp"
#include "wait_basedon_timestamp.hpp"
#include "mp4_box.hpp"

#include <map>

extern "C" {
void* make_mp4demux_streamer();
void destroy_mp4demux_streamer(void* streamer);
}

namespace cpp_streamer
{
class Mp4Demuxer : CppStreamerInterface
{
public:
    Mp4Demuxer();
    virtual ~Mp4Demuxer();

public:
    virtual std::string StreamerName() override;
    virtual void SetLogger(Logger* logger) override {
        logger_ = logger;
    }
    virtual int AddSinker(CppStreamerInterface* sinker) override;
    virtual int RemoveSinker(const std::string& name) override;
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) override;
    virtual void StartNetwork(const std::string& url, void* loop_handle) override {}
    virtual void AddOption(const std::string& key, const std::string& value) override;
    virtual void SetReporter(StreamerReport* reporter) override;

private:
    static std::map<std::string, std::string> def_options_;

private:
    DataBuffer buffer_;

private:
    FtypBox* ftyp_box_ = nullptr;
    MoovBox* moov_box_ = nullptr;
    FreeBox* free_box_ = nullptr;
    MdatBox* mdat_box_ = nullptr;
};

}

#endif //MP4_DEMUX_HPP