#ifndef MPEGTS_DEMUX_H
#define MPEGTS_DEMUX_H
#include "mpegts_pub.hpp"
#include "logger.hpp"
#include "data_buffer.hpp"
#include "cpp_streamer_interface.hpp"
#include "wait_basedon_timestamp.hpp"

#include <string>
#include <memory>
#include <map>

extern "C"
{
void* make_mpegtsdemux_streamer();
void destroy_mpegtsdemux_streamer(void* streamer);
}

namespace cpp_streamer
{
class MpegtsDemux : public CppStreamerInterface
{
public:
    MpegtsDemux();
    virtual ~MpegtsDemux();

    int Decode(DATA_BUFFER_PTR data_ptr);

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
    int DecodeUnit(unsigned char* data_p);
    bool IsPmt(unsigned short pmt_id);
    int PesParse(unsigned char* p, size_t npos, unsigned char** ret_pp, size_t& ret_size,
            uint64_t& dts, uint64_t& pts);
    void InsertIntoDatabuf(unsigned char* data_p, size_t data_size, unsigned short pid);
    void OnCallback(unsigned short pid, uint64_t dts, uint64_t pts);
    void ReportEvent(const std::string& type, const std::string& value);
    int GetMediaInfoByPid(uint16_t pid, MEDIA_PKT_TYPE& media_type, MEDIA_CODEC_TYPE& codec_type);
    void Output(Media_Packet_Ptr pkt_ptr);
    void reportPat();
    void reportPmt();

private:
    PatInfo _pat;
    PmtInfo _pmt;
    std::vector<DATA_BUFFER_PTR> _data_buffer_vec;
    size_t _data_total;
    unsigned short _last_pid;
    uint64_t _last_dts;
    uint64_t _last_pts;

private:
    static std::map<std::string, std::string> def_options_;

private:
    WaitBasedOnTimestamp waiter_;

private:
    int64_t opus_count_ = 0;
    int64_t opus_dts_ = -1;
};

typedef std::shared_ptr<MpegtsDemux> TS_DEMUX_PTR;

}
#endif

