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

class Mpegts2MsPushStreamerMgr: public StreamerReport
{
public:
    Mpegts2MsPushStreamerMgr(const std::string& src_ts, 
            const std::string& output_url):ts_file_(src_ts)
                                           , dst_url_(output_url)
    {
    }
    virtual ~Mpegts2MsPushStreamerMgr()
    {
        mspush_ready_ = false;
        thread_ptr_->join();
        thread_ptr_ = nullptr;
    }

    int MakeStreamers(uv_loop_t* loop_handle) {
        loop_ = loop_handle;
        tsdemux_streamer_ = CppStreamerFactory::MakeStreamer("mpegtsdemux");
        if (!tsdemux_streamer_) {
            LogErrorf(logger_, "make streamer mpegtsdemux error");
            return -1;
        }
        LogInfof(logger_, "make mpegts demux streamer:%p, name:%s", 
                tsdemux_streamer_, tsdemux_streamer_->StreamerName().c_str());
        tsdemux_streamer_->SetLogger(logger_);
        tsdemux_streamer_->AddOption("re", "true");
        tsdemux_streamer_->SetReporter(this);
 
        mspush_streamer_ = CppStreamerFactory::MakeStreamer("mspush");
        if (!mspush_streamer_) {
            LogErrorf(logger_, "make streamer mspush error");
            return -1;
        }
        LogInfof(logger_, "make mspush streamer:%p, name:%s", mspush_streamer_, mspush_streamer_->StreamerName().c_str());
        mspush_streamer_->SetLogger(logger_);
        mspush_streamer_->SetReporter(this);
        LogInfof(logger_, "start network url:%s", dst_url_.c_str());
        mspush_streamer_->StartNetwork(dst_url_, loop_handle);

        tsdemux_streamer_->AddSinker(mspush_streamer_);
        return 0;

    }

    void Start() {
        if (!thread_ptr_) {
            thread_ptr_ = std::make_shared<std::thread>(&Mpegts2MsPushStreamerMgr::OnWork, this);
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
        if (type == "audio_produce") {
            if (value == "ready") {
                mspush_ready_ = true;
            }
        }
        if (type == "error") {
            mspush_ready_ = false;
        }
    }

protected:
    int InputTsData(uint8_t* data, size_t data_len) {
        if (!tsdemux_streamer_) {
            LogErrorf(logger_, "mpegts demux streamer is not ready");
            return -1;
        }
        //LogInfof(logger_, "input data len:%u", data_len);
        Media_Packet_Ptr pkt_ptr = std::make_shared<Media_Packet>();
        pkt_ptr->buffer_ptr_->AppendData((char*)data, data_len);
        tsdemux_streamer_->SourceData(pkt_ptr);
        return 0;
    }

    void OnWork() {
        int64_t start_ms = now_millisec();

        while(!mspush_ready_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            int64_t now_ms = now_millisec();

            if (now_ms - start_ms > 10*1000) {
                LogErrorf(logger_, "mspush session is not ready and end the job");
                return;
            }
        }
        LogInfof(logger_, "mspush session is ready");

        FILE* file_p = fopen(ts_file_.c_str(), "r");
        if (!file_p) {
            LogErrorf(s_logger, "open mpegts file error:%s", ts_file_.c_str());
            return;
        }
        uint8_t read_data[10*188];
        size_t read_n = 0;
        do {
            read_n = fread(read_data, 1, sizeof(read_data), file_p);
            if (read_n > 0) {
                InputTsData(read_data, read_n);
            }
            if (!mspush_ready_) {
                LogErrorf(logger_, "mspush error and break mpegts reading");
                break;
            }
        } while (read_n > 0);
        fclose(file_p);
        LogInfof(logger_, "mpegts read is over...");

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        delete tsdemux_streamer_;
        delete mspush_streamer_;
        uv_loop_close(loop_);
        LogInfof(logger_, "job is done.");
        exit(0);
    }

private:
    std::string ts_file_;
    std::string dst_url_;
    uv_loop_t* loop_ = nullptr;
    std::shared_ptr<std::thread> thread_ptr_;
    bool mspush_ready_ = false;

private:
    Logger* logger_ = nullptr;
    CppStreamerInterface* mspush_streamer_ = nullptr;
    CppStreamerInterface* tsdemux_streamer_    = nullptr;
};

int main(int argc, char** argv) {
    char input_ts_name[516];
    char output_url_name[516];
    char log_file[516];

    int opt = 0;
    bool input_ts_name_ready = false;
    bool output_url_name_ready = false;
    bool log_file_ready = false;

    while ((opt = getopt(argc, argv, "i:o:l:h")) != -1) {
        switch (opt) {
            case 'i': strncpy(input_ts_name, optarg, sizeof(input_ts_name)); input_ts_name_ready = true; break;
            //./mediasoup_push_demo -i ~/movies/webrtc.ts -o "https://webrtcserver.com.cn:4443?roomId=200&userId=1006"
            case 'o': strncpy(output_url_name, optarg, sizeof(output_url_name)); output_url_name_ready = true; break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-i input mpegts(h264+opus) file]\n\
    [-o mediasoup server url, eg: https://xxxx.com:443]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!input_ts_name_ready || !output_url_name_ready) {
        std::cout << "please input mpegts file/output mspush url\r\n";
        return -1;
    }

    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }

    CppStreamerFactory::SetLogger(s_logger);
    CppStreamerFactory::SetLibPath("./output/lib");

    LogInfof(s_logger, "mpegts2mspush streamer manager is starting, input mpegts:%s, output mspush url:%s",
            input_ts_name, output_url_name);
    uv_loop_t* loop = uv_default_loop();
 
    std::shared_ptr<Mpegts2MsPushStreamerMgr> mgr_ptr = std::make_shared<Mpegts2MsPushStreamerMgr>(input_ts_name, output_url_name);

    mgr_ptr->SetLogger(s_logger);

    if (mgr_ptr->MakeStreamers(loop) < 0) {
        LogErrorf(s_logger, "call mpegts2mspush streamer error");
        return -1;
    }
    LogInfof(s_logger, "mpegts to mspush is starting......");
    mgr_ptr->Start();
    while (true) {
        uv_run(loop, UV_RUN_DEFAULT);
    }
    return 0;
}


