#ifndef MP4_DEMUX_HPP
#define MP4_DEMUX_HPP
#include "data_buffer.hpp"
#include "cpp_streamer_interface.hpp"
#include "logger.hpp"
#include "wait_basedon_timestamp.hpp"
#include "mp4_box.hpp"

#include <map>
#include <vector>

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
    void OnRead();
    void Output(Media_Packet_Ptr pkt_ptr);
    void makeMovItems();
    void getSampleInfoInChunk(uint32_t chunk_offset, size_t chunk_index,
        uint32_t sample_per_chunk, uint32_t sample_index, int64_t& dts,
        const TrakInfo& trakinfo);
    int64_t getDurationBySampleIndex(uint32_t sample_index,
        const TrakInfo& trakinfo);
    int64_t getPtsBySampleIndex(uint32_t sample_index, int64_t dts,
        const TrakInfo& trakinfo);
    bool isKeyframe(uint32_t sample_index, const TrakInfo& trakinfo);
    void adjustAllDts();
    void handleMovItems();
    void handleH264SpsPps(const TrakInfo& trakinfo);
    void handleH265VpsSpsPps(const TrakInfo& trakinfo);
    void handleAACExtraData(const TrakInfo& trakinfo);
    void sendMediaPacket(MEDIA_PKT_TYPE av_type, MEDIA_CODEC_TYPE codec_type,
        int64_t dts, int64_t pts, 
        bool is_keyframe, bool is_seqhdr,
        uint8_t* data, size_t len);

private:
    static std::map<std::string, std::string> def_options_;

private:
    DataBuffer buffer_;

private:
    MovInfo mov_;
    FtypBox* ftyp_box_ = nullptr;
    MoovBox* moov_box_ = nullptr;
    FreeBox* free_box_ = nullptr;
    MdatBox* mdat_box_ = nullptr;
    std::vector<Mp4BoxBase*> unknown_boxes_;
    std::multimap<int64_t, MovItem> sample_items_;
    IoReadInterface* io_reader_ = nullptr;

private:
    WaitBasedOnTimestamp waiter_;
};

}

#endif //MP4_DEMUX_HPP