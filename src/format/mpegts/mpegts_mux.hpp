#ifndef MPEGTS_MUX_HPP
#define MPEGTS_MUX_HPP
#include "mpegts_pub.hpp"
#include "data_buffer.hpp"
#include "utils/av/av.hpp"
#include "cpp_streamer_interface.hpp"
#include <map>
#include <queue>

extern "C"
{
void* make_mpegtsmux_streamer();
void destroy_mpegtsmux_streamer(void* streamer);
}
namespace cpp_streamer
{
class MpegtsMux : public CppStreamerInterface
{
public:
    MpegtsMux();
    virtual ~MpegtsMux();

public:
    static uint8_t* GetH264AudData(size_t& len);
    static uint8_t* GetH265AudData(size_t& len);

public:
    int InputPacket(Media_Packet_Ptr pkt_ptr);
    void SetVideoFlag(bool flag) { has_video_ = flag;}
    bool HasVideo() { return has_video_; }
    void SetAudioFlag(bool flag) { has_audio_ = flag;}
    bool HasAudio() { return has_audio_; }

    void SetVideoCodec(MEDIA_CODEC_TYPE codec) { video_codec_type_ = codec; }
    void SetAudioCodec(MEDIA_CODEC_TYPE codec) { audio_codec_type_ = codec; }
    MEDIA_CODEC_TYPE GetVideoCodec() { return video_codec_type_; }
    MEDIA_CODEC_TYPE GetAudioCodec() { return audio_codec_type_; }

public:
    virtual std::string StreamerName() override;
    virtual void SetLogger(Logger* logger) override;
    virtual int AddSinker(CppStreamerInterface* sinker) override;
    virtual int RemoveSinker(const std::string& name) override;
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) override;
    virtual void StartNetwork(const std::string& url, void* loop_handle) override;
    virtual void AddOption(const std::string& key, const std::string& value) override;
    virtual void SetReporter(StreamerReport* reporter) override;

private:
    int WritePat();
    int WritePmt();
    int GeneratePat();
    int GeneratePmt();
    int WritePes(Media_Packet_Ptr pkt_ptr);
    int WritePesHeader(int64_t data_size,
                    bool is_video, int64_t dts, int64_t pts);
    int WriteTs(uint8_t* data, uint8_t flag, int64_t ts);
    int WritePcr(uint8_t* data, int64_t ts);
    int AdaptationBufInit(uint8_t* data, uint8_t data_size, uint8_t remain_bytes);

private:
    int HandlePacket(Media_Packet_Ptr pkt_ptr);
    int HandleVideo(Media_Packet_Ptr pkt_ptr);
    int HandleAudio(Media_Packet_Ptr pkt_ptr);
    void TsOutput(Media_Packet_Ptr pkt_ptr, uint8_t* data);

    int HandleH264(Media_Packet_Ptr pkt_ptr);
    int HandleH265(Media_Packet_Ptr pkt_ptr);
    int HandleVpx(Media_Packet_Ptr pkt_ptr);

    int HandleAudioAac(Media_Packet_Ptr pkt_ptr);
    int HandleAudioOpus(Media_Packet_Ptr pkt_ptr);

    void ReportEvent(const std::string& type, const std::string& value);

private:
    static uint8_t H264_AUD_DATA[];
    static uint8_t H265_AUD_DATA[];

private:
    uint32_t pmt_count_     = 1;
    uint8_t pat_data_[TS_PACKET_SIZE];
    uint8_t pmt_data_[TS_PACKET_SIZE];
    uint8_t pes_header_[TS_PACKET_SIZE];

private://about pat
    uint16_t transport_stream_id_ = 1;
    uint8_t  pat_ver_ = 0;
    uint16_t program_number_ = 1;

private://about pmt
    bool has_video_ = true;
    bool has_audio_ = true;
    MEDIA_CODEC_TYPE video_codec_type_ = MEDIA_CODEC_H264;
    MEDIA_CODEC_TYPE audio_codec_type_ = MEDIA_CODEC_AAC;

    uint16_t pmt_pid_    = 0x1001;
    uint16_t pmt_pn      = 0x01;
    uint8_t  pmt_ver_    = 0;
    uint16_t pcr_id_     = 0x100;//256
    uint16_t pminfo_len_ = 0;
    
    uint16_t video_stream_type_ = STREAM_TYPE_VIDEO_H264;
    uint16_t audio_stream_type_ = STREAM_TYPE_AUDIO_AAC;
    uint16_t video_pid_ = 0x100;//256
    uint16_t audio_pid_ = 0x101;//257
    uint8_t video_sid_ = 0;
    uint8_t audio_sid_ = 0;

    int64_t pes_header_size_ = 0;
    uint8_t video_cc_ = 0;
    uint8_t audio_cc_ = 0;
    int64_t last_patpmt_ts_ = -1;
    int64_t patpmt_interval_ = 3000;//3000ms default

private:
    uint8_t pps_[512];
    size_t pps_len_ = 0;
    uint8_t sps_[512];
    size_t sps_len_ = 0;
    uint8_t vps_[512];
    size_t vps_len_ = 0;

private:
    uint8_t aac_type_ = 0;
    int sample_rate_  = 0;
    uint8_t channel_  = 0;

private:
    bool video_ready_ = false;
    bool audio_ready_ = false;
    bool ready_       = false;
    std::queue<Media_Packet_Ptr> wait_queue_;
};

}

#endif
