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

class MsPull2MpegtsStreamerMgr: public StreamerReport, public CppStreamerInterface
{
public:
    MsPull2MpegtsStreamerMgr(const std::string& src_url, 
            const std::string& output_ts):ts_file_(output_ts)
                                           , src_url_(src_url)
    {
    }
    virtual ~MsPull2MpegtsStreamerMgr()
    {
        mspull_ready_ = false;
    }

    int MakeStreamers(uv_loop_t* loop_handle) {
        loop_ = loop_handle;

        mspull_streamer_ = CppStreamerFactory::MakeStreamer("mspull");
        mspull_streamer_->SetLogger(logger_);
        mspull_streamer_->SetReporter(this);

        ts_streamer_ = CppStreamerFactory::MakeStreamer("mpegtsmux");
        ts_streamer_->SetLogger(logger_);
        ts_streamer_->SetReporter(this);

        timesync_streamer_ = CppStreamerFactory::MakeStreamer("timesync");
        timesync_streamer_->SetLogger(logger_);
        timesync_streamer_->SetReporter(this);

        mspull_streamer_->AddSinker(timesync_streamer_);
        timesync_streamer_->AddSinker(ts_streamer_);
        ts_streamer_->AddSinker(this);

        return 0;

    }

    void Start() {
        LogInfof(logger_, "start network url:%s", src_url_.c_str());
        try {
            mspull_streamer_->StartNetwork(src_url_, loop_);
        } catch(CppStreamException& e) {
            LogErrorf(logger_, "mspull start network exception:%s", e.what());
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
                mspull_ready_ = true;
            }
        }
        if (type == "error") {
            mspull_ready_ = false;
        }
    }

public:
    virtual std::string StreamerName() override {
        return "mspull2mpegts";
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
        FILE* file_p = fopen(ts_file_.c_str(), "ab+");
        if (file_p) {
            fwrite(pkt_ptr->buffer_ptr_->Data(), 1, pkt_ptr->buffer_ptr_->DataLen(), file_p);
            fclose(file_p);
        }

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
    bool mspull_ready_ = false;

private:
    Logger* logger_ = nullptr;
    CppStreamerInterface* mspull_streamer_   = nullptr;
    CppStreamerInterface* timesync_streamer_ = nullptr;
    CppStreamerInterface* ts_streamer_       = nullptr;
};

/*
 ./mediasoup_pull_demo -i "https://xxxxx.com?roomId=100&userId=1000&vpid=xxxxx&apid=xxxx" -o 1000.ts
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
            /*eg: "https://xxxxx.com?roomId=100&userId=1000&vpid=xxxx&apid=xxxx"*/
            case 'i': strncpy(input_url_name, optarg, sizeof(input_url_name)); input_url_name_ready = true; break;
            case 'o': strncpy(output_ts_name, optarg, sizeof(output_ts_name)); output_ts_name_ready = true; break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-i input mediasoup broadcaster pull url]\n\
    [-o mpegts(h264+opus) file]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!input_url_name_ready || !output_ts_name_ready) {
        std::cout << "please mspull/input mpegts file\r\n";
        return -1;
    }

    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }

    CppStreamerFactory::SetLogger(s_logger);
    CppStreamerFactory::SetLibPath("./output/lib");

    LogInfof(s_logger, "mspull2mpegts streamer manager is starting, input mspull:%s, output mpegts:%s",
            input_url_name, output_ts_name);
    uv_loop_t* loop = uv_default_loop();

    std::shared_ptr<MsPull2MpegtsStreamerMgr> mgr_ptr = std::make_shared<MsPull2MpegtsStreamerMgr>(input_url_name, output_ts_name);

    mgr_ptr->SetLogger(s_logger);

    if (mgr_ptr->MakeStreamers(loop) < 0) {
        LogErrorf(s_logger, "call mspull2mpegts streamer error");
        return -1;
    }
    LogInfof(s_logger, "mspull to mpegts is starting......");
    mgr_ptr->Start();
    while (true) {
        uv_run(loop, UV_RUN_DEFAULT);
    }
 
    return 0;
}
