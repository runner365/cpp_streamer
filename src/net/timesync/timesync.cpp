#include "timesync.hpp"
#include "cpp_streamer_interface.hpp"

namespace cpp_streamer
{

class TimeSync : public CppStreamerInterface
{
public:
    TimeSync();
    virtual ~TimeSync();

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
    void HandleVideoPacket(Media_Packet_Ptr pkt_ptr);
    void HandleAudioPacket(Media_Packet_Ptr pkt_ptr);
    void OutputPacket(Media_Packet_Ptr pkt_ptr);

private:
    int64_t last_video_dts_ = -1;
    int64_t last_video_pts_ = -1;
    int64_t video_dts_      = 0;
    int64_t video_pts_      = 0;

    int64_t last_audio_dts_ = -1;
    int64_t audio_dts_      = -1;
};

}
