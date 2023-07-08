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
#include <memory>

using namespace cpp_streamer;

static Logger* s_logger = nullptr;

class Flv2RtmpPublishStreamerMgr : public StreamerReport
{
public:
    Flv2RtmpPublishStreamerMgr(const std::string& src_flv, 
            const std::string& output_url):src_flv_(src_flv)
                                           , dst_url_(output_url)
    {
    }
    virtual ~Flv2RtmpPublishStreamerMgr()
    {
        rtmp_ready_ = false;
        thread_ptr_->join();
        thread_ptr_ = nullptr;
    }

    int MakeStreamers(uv_loop_t* loop_handle) {
        loop_ = loop_handle;
        flvdemux_streamer_ = CppStreamerFactory::MakeStreamer("flvdemux");
        if (!flvdemux_streamer_) {
            LogErrorf(logger_, "make streamer flvdemux error");
            return -1;
        }
        LogInfof(logger_, "make flv demux streamer:%p, name:%s", 
                flvdemux_streamer_, flvdemux_streamer_->StreamerName().c_str());
        flvdemux_streamer_->SetLogger(logger_);
        flvdemux_streamer_->AddOption("re", "true");
        flvdemux_streamer_->SetReporter(this);
 
        rtmppublish_streamer_ = CppStreamerFactory::MakeStreamer("rtmppublish");
        if (!rtmppublish_streamer_) {
            LogErrorf(logger_, "make streamer rtmppublish error");
            return -1;
        }
        LogInfof(logger_, "make rtmppublish streamer:%p, name:%s", rtmppublish_streamer_, rtmppublish_streamer_->StreamerName().c_str());
        rtmppublish_streamer_->SetLogger(logger_);
        rtmppublish_streamer_->SetReporter(this);
        rtmppublish_streamer_->StartNetwork(dst_url_, loop_handle);

        flvdemux_streamer_->AddSinker(rtmppublish_streamer_);
        return 0;

    }

    void Start() {
        if (!thread_ptr_) {
            thread_ptr_ = std::make_shared<std::thread>(&Flv2RtmpPublishStreamerMgr::OnWork, this);
        }
    }

    void SetLogger(Logger* logger) {
        logger_ = logger;
    }

public:
    virtual void OnReport(const std::string& name,
            const std::string& type,
            const std::string& value) override {
        LogWarnf(logger_, "report name:%s, type:%s, value:%s",
                name.c_str(), type.c_str(), value.c_str());
        if (type == "event") {
            if (value == "publish") {
                rtmp_ready_ = true;
            }
            if (value == "close") {
                rtmp_ready_ = false;
            }
        }
        if (type == "error") {
            rtmp_ready_ = false;
        }
    }

protected:
    int InputFlvData(uint8_t* data, size_t data_len) {
        if (!flvdemux_streamer_) {
            LogErrorf(logger_, "flv demux streamer is not ready");
            return -1;
        }
        //LogInfof(logger_, "input data len:%u", data_len);
        Media_Packet_Ptr pkt_ptr = std::make_shared<Media_Packet>();
        pkt_ptr->buffer_ptr_->AppendData((char*)data, data_len);
        flvdemux_streamer_->SourceData(pkt_ptr);
        return 0;
    }

    void OnWork() {
        int wait_count = 0;

        while(!rtmp_ready_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (wait_count++ > 5) {
                LogErrorf(logger_, "rtmp session is not ready and end the job");
                return;
            }
        }
        LogInfof(logger_, "rtmp is ready");

        FILE* file_p = fopen(src_flv_.c_str(), "r");
        if (!file_p) {
            LogErrorf(s_logger, "open flv file error:%s", src_flv_.c_str());
            return;
        }
        uint8_t read_data[2048];
        size_t read_n = 0;
        do {
            read_n = fread(read_data, 1, sizeof(read_data), file_p);
            if (read_n > 0) {
                InputFlvData(read_data, read_n);
            }
            if (!rtmp_ready_) {
                LogErrorf(logger_, "rtmp publish error and break flv reading");
                break;
            }
        } while (read_n > 0);
        fclose(file_p);
        LogInfof(logger_, "flv read is over...");
        uv_loop_close(loop_);
    }

private:
    std::string src_flv_;
    std::string dst_url_;
    uv_loop_t* loop_ = nullptr;
    std::shared_ptr<std::thread> thread_ptr_;
    bool rtmp_ready_ = false;

private:
    Logger* logger_ = nullptr;
    CppStreamerInterface* rtmppublish_streamer_ = nullptr;
    CppStreamerInterface* flvdemux_streamer_    = nullptr;
};

int main(int argc, char** argv) {
    char input_flv_name[128];
    char output_url_name[128];
    char log_file[128];

    int opt = 0;
    bool input_flv_name_ready = false;
    bool output_url_name_ready = false;
    bool log_file_ready = false;

    while ((opt = getopt(argc, argv, "i:o:l:h")) != -1) {
        switch (opt) {
            case 'i': strncpy(input_flv_name, optarg, sizeof(input_flv_name)); input_flv_name_ready = true; break;
            case 'o': strncpy(output_url_name, optarg, sizeof(output_url_name)); output_url_name_ready = true; break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-i input flv file]\n\
    [-o rtmp publish url]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!input_flv_name_ready || !output_url_name_ready) {
        std::cout << "please input flv file/output rtmp url\r\n";
        return -1;
    }


    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }

    CppStreamerFactory::SetLogger(s_logger);
    CppStreamerFactory::SetLibPath("./output/lib");


    LogInfof(s_logger, "flv2rtmppublish streamer manager is starting, input flv:%s, output rtmp url:%s",
            input_flv_name, output_url_name);
     uv_loop_t* loop = uv_default_loop();
    auto streamer_mgr_ptr = std::make_shared<Flv2RtmpPublishStreamerMgr>(std::string(input_flv_name),
            std::string(output_url_name));
    streamer_mgr_ptr->SetLogger(s_logger);

    if (streamer_mgr_ptr->MakeStreamers(loop) < 0) {
        LogErrorf(s_logger, "call flv2rtmppublish streamer error");
        return -1;
    }

    streamer_mgr_ptr->Start();
    while (true) {
        uv_run(loop, UV_RUN_DEFAULT);
    }
    return 0;
 
}

