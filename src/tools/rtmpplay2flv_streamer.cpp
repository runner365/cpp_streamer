#include "cpp_streamer_factory.hpp"
#include "logger.hpp"
#include <uv.h>
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

class RtmpPlay2FlvStreamerMgr : public CppStreamerInterface, public StreamerReport
{
public:
    RtmpPlay2FlvStreamerMgr(const std::string& src_url, const std::string& output_flv):src_url_(src_url)
    , output_flv_(output_flv)
    {
    }
    virtual ~RtmpPlay2FlvStreamerMgr()
    {
    }

public:
    int MakeStreamers(uv_loop_t* loop_handle) {
        flv_mux_streamer_ = CppStreamerFactory::MakeStreamer("flvmux");
        if (!flv_mux_streamer_) {
            LogErrorf(logger_, "make streamer flvmux error");
            return -1;
        }
        LogInfof(logger_, "make flv mux streamer:%p, name:%s", flv_mux_streamer_, flv_mux_streamer_->StreamerName().c_str());
        flv_mux_streamer_->SetLogger(logger_);
        flv_mux_streamer_->SetReporter(this);
        flv_mux_streamer_->AddSinker(this);
 
        rtmpplay_streamer_ = CppStreamerFactory::MakeStreamer("rtmpplay");
        if (!rtmpplay_streamer_) {
            LogErrorf(logger_, "make streamer rtmpplay error");
            return -1;
        }
        LogInfof(logger_, "make rtmpplay streamer:%p, name:%s", rtmpplay_streamer_, rtmpplay_streamer_->StreamerName().c_str());
        rtmpplay_streamer_->SetLogger(logger_);
        rtmpplay_streamer_->SetReporter(this);
        rtmpplay_streamer_->AddSinker(flv_mux_streamer_);
        
        rtmpplay_streamer_->StartNetwork(src_url_, loop_handle);
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
        return "";
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
        LogDebugf(logger_, "flv mux output pcket %s", pkt_ptr->Dump().c_str());
        FILE* file_p = fopen(output_flv_.c_str(), "ab+");
        if (file_p) {
            fwrite(pkt_ptr->buffer_ptr_->Data(), 1, pkt_ptr->buffer_ptr_->DataLen(), file_p);
            fclose(file_p);
        }
        return 0;
    }

    virtual void StartNetwork(const std::string& url, void* loop_handle) override {
    }

    virtual void AddOption(const std::string& key, const std::string& value) override{
        return;
    }

    virtual void SetReporter(StreamerReport* reporter) override {

    }

private:
    std::string src_url_;
    std::string output_flv_;

private:
    Logger* logger_ = nullptr;
    CppStreamerInterface* rtmpplay_streamer_ = nullptr;
    CppStreamerInterface* flv_mux_streamer_  = nullptr;
};

int main(int argc, char** argv) {
    char input_url_name[128];
    char output_flv_name[128];
    char log_file[128];

    int opt = 0;
    bool input_url_name_ready = false;
    bool output_flv_name_ready = false;
    bool log_file_ready = false;

    while ((opt = getopt(argc, argv, "i:o:l:h")) != -1) {
        switch (opt) {
            case 'i': strncpy(input_url_name, optarg, sizeof(input_url_name)); input_url_name_ready = true; break;
            case 'o': strncpy(output_flv_name, optarg, sizeof(output_flv_name)); output_flv_name_ready = true; break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-i input rtmp url]\n\
    [-o flv file name]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!input_url_name_ready || !output_flv_name_ready) {
        std::cout << "please input url/output flv name\r\n";
        return -1;
    }


    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }

    CppStreamerFactory::SetLogger(s_logger);
    CppStreamerFactory::SetLibPath("./output/lib");


    LogInfof(s_logger, "rtmpplay2flv streamer manager is starting, input url:%s, output filename:%s",
            input_url_name, output_flv_name);
    uv_loop_t* loop = uv_default_loop();
    auto streamer_mgr_ptr = std::make_shared<RtmpPlay2FlvStreamerMgr>(std::string(input_url_name),
            std::string(output_flv_name));
    streamer_mgr_ptr->SetLogger(s_logger);

    if (streamer_mgr_ptr->MakeStreamers(loop) < 0) {
        LogErrorf(s_logger, "call rtmpplay2flv streamer error");
        return -1;
    }

    while (true) {
        uv_run(loop, UV_RUN_DEFAULT);
    }
    return 0;
 } 
