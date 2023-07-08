#include "cpp_streamer_interface.hpp"
#include "cpp_streamer_factory.hpp"
#include "logger.hpp"
#include "media_packet.hpp"

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <unistd.h>
#include <chrono>
#include <thread>

using namespace cpp_streamer;

static Logger* s_logger = nullptr;


class MpegTsDump : public CppStreamerInterface, public StreamerReport
{
public:
    MpegTsDump()
    {
    }
    virtual ~MpegTsDump()
    {
        if (ts_demux_streamer_) {
            delete ts_demux_streamer_;
            ts_demux_streamer_ = nullptr;
        }
    }

public:
    int MakeStreamers() {
        ts_demux_streamer_ = CppStreamerFactory::MakeStreamer("mpegtsdemux");
        if (!ts_demux_streamer_) {
            LogErrorf(logger_, "make streamer flvdemux error");
            return -1;
        }
        LogInfof(logger_, "make mpegts demux streamer:%p, name:%s",
                ts_demux_streamer_, ts_demux_streamer_->StreamerName().c_str());
        ts_demux_streamer_->SetLogger(logger_);
        ts_demux_streamer_->SetReporter(this);
        ts_demux_streamer_->AddSinker(this);
        return 0;
    }

    int InputTsData(uint8_t* data, size_t data_len) {
        if (!ts_demux_streamer_) {
            LogErrorf(logger_, "ts demux streamer is not ready");
            return -1;
        }
        Media_Packet_Ptr pkt_ptr = std::make_shared<Media_Packet>();
        pkt_ptr->buffer_ptr_->AppendData((char*)data, data_len);
        ts_demux_streamer_->SourceData(pkt_ptr);
        return 0;
    }

public:
    virtual void OnReport(const std::string& name,
            const std::string& type,
            const std::string& value) override {
        LogWarnf(logger_, "report name:%s, type:%s, value:%s",
                name.c_str(), type.c_str(), value.c_str());
    }

public:
    virtual std::string StreamerName() override {
        return "mpegts_dump";
    }
    virtual void SetLogger(Logger* logger) override {
        logger_ = logger;
    }
    virtual int AddSinker(CppStreamerInterface* sinker) override {
        return 0;
    }
    virtual int RemoveSinker(const std::string& name) override {
        return 0;
    }
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) override {
        LogInfof(logger_, "media packet:%s", pkt_ptr->Dump().c_str());
        return 0;
    }
    virtual void StartNetwork(const std::string& url, void* loop_handle) override {
        return;
    }
    virtual void AddOption(const std::string& key, const std::string& value) override {
        return;
    }
    virtual void SetReporter(StreamerReport* reporter) override {

    }

private:
    Logger* logger_ = nullptr;
    CppStreamerInterface* ts_demux_streamer_ = nullptr;
};

int main(int argc, char** argv) {
    char input_ts_name[128];
    char log_file[128];

    int opt = 0;
    bool input_ts_name_ready = false;
    bool log_file_ready = false;

    while ((opt = getopt(argc, argv, "i:l:h")) != -1) {
        switch (opt) {
            case 'i': strncpy(input_ts_name, optarg, sizeof(input_ts_name)); input_ts_name_ready = true; break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-i mpegts file name]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!input_ts_name_ready) {
        std::cout << "please input ts name\r\n";
        return -1;
    }

    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }

    CppStreamerFactory::SetLogger(s_logger);
    CppStreamerFactory::SetLibPath("./output/lib");

    LogInfof(s_logger, "mpegts dump is starting, input filename:%s",
            input_ts_name);
 
    auto streamer_mgr_ptr = std::make_shared<MpegTsDump>();

    streamer_mgr_ptr->SetLogger(s_logger);
    if (streamer_mgr_ptr->MakeStreamers() < 0) {
        LogErrorf(s_logger, "call MakeStreamers error");
        return -1;
    }
    FILE* file_p = fopen(input_ts_name, "r");
    if (!file_p) {
        LogErrorf(s_logger, "open ts file error:%s", input_ts_name);
        return -1;
    }
    uint8_t read_data[188 * 10];
    size_t read_n = 0;
    do {
        read_n = fread(read_data, 1, sizeof(read_data), file_p);
        if (read_n > 0) {
            streamer_mgr_ptr->InputTsData(read_data, read_n);
        }
    } while (read_n > 0);

    LogInfof(s_logger, "mpegts dump done");

    streamer_mgr_ptr = nullptr;
    CppStreamerFactory::ReleaseAll();
    
    getchar();
    delete s_logger;

    return 0;


}


