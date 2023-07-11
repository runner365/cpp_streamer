#include "logger.hpp"
#include "cpp_streamer_factory.hpp"

#include <iostream>
#include <uv.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <memory>

using namespace cpp_streamer;

static Logger* s_logger = nullptr;

class Whep2MpegtsStreamerMgr: public StreamerReport, public CppStreamerInterface
{
public:
    Whep2MpegtsStreamerMgr(const std::string& src_url, 
            const std::string& output_ts):ts_file_(output_ts)
                                           , src_url_(src_url)
    {
    }
    virtual ~Whep2MpegtsStreamerMgr()
    {
        whep_ready_ = false;
    }

    int MakeStreamers(uv_loop_t* loop_handle) {
        loop_ = loop_handle;

        whep_streamer_ = CppStreamerFactory::MakeStreamer("whep");
        if (!whep_streamer_) {
            LogErrorf(logger_, "make streamer whep error");
            return -1;
        }
        LogInfof(logger_, "make whep streamer:%p, name:%s", whep_streamer_, whep_streamer_->StreamerName().c_str());
        whep_streamer_->SetLogger(logger_);
        whep_streamer_->SetReporter(this);
        whep_streamer_->AddSinker(this);

        return 0;

    }

    void Start() {
        LogInfof(logger_, "start network url:%s", src_url_.c_str());
        try {
            whep_streamer_->StartNetwork(src_url_, loop_);
        } catch(CppStreamException& e) {
            LogErrorf(logger_, "whep start network exception:%s", e.what());
        }
    }

public:
    virtual void OnReport(const std::string& name,
            const std::string& type,
            const std::string& value) override {
        LogWarnf(logger_, "report name:%s, type:%s, value:%s",
                name.c_str(), type.c_str(), value.c_str());
        if (type == "dtls") {
            if (value == "ready") {
                whep_ready_ = true;
            }
        }
        if (type == "error") {
            whep_ready_ = false;
        }
    }

public:
    virtual std::string StreamerName() override {
        return "whep2mpegts";
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
        LogInfof(logger_, "whep media packet:%s", pkt_ptr->Dump().c_str());
        return 0;
    }
    virtual void StartNetwork(const std::string& url, void* loop_handle) override {

    }
    virtual void AddOption(const std::string& key, const std::string& value) override {

    }
    virtual void SetReporter(StreamerReport* reporter) override {

    }


private:
    std::string ts_file_;
    std::string src_url_;
    uv_loop_t* loop_ = nullptr;
    bool whep_ready_ = false;

private:
    Logger* logger_ = nullptr;
    CppStreamerInterface* whep_streamer_ = nullptr;
};

/*
 ./whep_srs_demo -i "http://10.0.8.5:1985/rtc/v1/whip-play/?app=live&stream=1000" -o 100.ts
 */
int main(int argc, char** argv) {
    char input_url_name[516];
    char output_ts_name[516];
    char log_file[516];

    int opt = 0;
    bool input_url_name_ready = false;
    bool output_ts_name_ready = false;
    bool log_file_ready = false;

    while ((opt = getopt(argc, argv, "i:o:l:h")) != -1) {
        switch (opt) {
            /*eg: http://10.0.24.12:1985/rtc/v1/whip-play/?app=live&stream=1000*/
            case 'i': strncpy(input_url_name, optarg, sizeof(input_url_name)); input_url_name_ready = true; break;
            case 'o': strncpy(output_ts_name, optarg, sizeof(output_ts_name)); output_ts_name_ready = true; break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-i input whep url]\n\
    [-o mpegts(h264+opus) file]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!input_url_name_ready || !output_ts_name_ready) {
        std::cout << "please output whep/input mpegts file\r\n";
        return -1;
    }

    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }

    CppStreamerFactory::SetLogger(s_logger);
    CppStreamerFactory::SetLibPath("./output/lib");

    LogInfof(s_logger, "whep2mpegts streamer manager is starting, input whep:%s, output mpegts:%s",
            input_url_name, output_ts_name);
    uv_loop_t* loop = uv_default_loop();

    std::shared_ptr<Whep2MpegtsStreamerMgr> mgr_ptr = std::make_shared<Whep2MpegtsStreamerMgr>(input_url_name, output_ts_name);

    mgr_ptr->SetLogger(s_logger);

    if (mgr_ptr->MakeStreamers(loop) < 0) {
        LogErrorf(s_logger, "call whep2mpegts streamer error");
        return -1;
    }
    LogInfof(s_logger, "whep to mpegts is starting......");
    mgr_ptr->Start();
    while (true) {
        uv_run(loop, UV_RUN_DEFAULT);
    }
 
    return 0;
}
