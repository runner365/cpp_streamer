#include "logger.hpp"
#include "cpp_streamer_factory.hpp"
#include "timer.hpp"

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
#include <vector>

using namespace cpp_streamer;

static Logger* s_logger = nullptr;
static const int BENCH_MAX = 100;
static const size_t WHIPS_INTERVAL = 10;

void CloseCallback(uv_async_t *handle);

class Mpegts2MediaSoupPushs: public StreamerReport, public TimerInterface
{
public:
    Mpegts2MediaSoupPushs(uv_loop_t* loop,
            const std::string& src_ts, 
            const std::string& output_url,
            size_t bench_count):TimerInterface(loop, 500)
                            , src_ts_(src_ts)
                            , base_url_(output_url)
                            , bench_count_(bench_count)
    {
    }
    virtual ~Mpegts2MediaSoupPushs()
    {
        StopTimer();
        thread_ptr_->join();
        thread_ptr_ = nullptr;
    }

//TimerInterface
protected:
    virtual void OnTimer() override {
        StartWhips();
    }

    std::string GetUrl(size_t index) {
        std::string url = base_url_;
        url += "_";
        url += std::to_string(index);
        return url;
    }

public:
    int MakeStreamers(uv_loop_t* loop_handle) {
        loop_ = loop_handle;
        uv_async_init(loop_, &async_, CloseCallback);

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

        for (size_t i = 0; i < bench_count_; i++) {
            CppStreamerInterface* mediasoup_pusher = CppStreamerFactory::MakeStreamer("mspush");
            if (!mediasoup_pusher) {
                LogErrorf(logger_, "make streamer mediasoup push error");
                return -1;
            }
            mediasoup_pusher->SetLogger(logger_);
            mediasoup_pusher->SetReporter(this);
            tsdemux_streamer_->AddSinker(mediasoup_pusher);

            mediasoup_pusher_vec.push_back(mediasoup_pusher);
        }

        return 0;

    }

    void StartWhips() {
        if (post_done_) {
            return;
        }

        LogWarnf(logger_, "StartWhips whip index:%lu",
                whip_index_);
        size_t i = 0;
        for (i = whip_index_; i < whip_index_ + WHIPS_INTERVAL;i++) {
            if (i >= bench_count_) {
                break;
            }
            std::string url = GetUrl(i);
            LogWarnf(logger_, "start network url:%s", url.c_str());
            mediasoup_pusher_vec[i]->StartNetwork(url, loop_);
        }
        whip_index_ = i;
        if (whip_index_ >= bench_count_) {
            post_done_ = true;
        }
    }

    void Start() {
        if (!thread_ptr_) {
            thread_ptr_ = std::make_shared<std::thread>(&Mpegts2MediaSoupPushs::OnWork, this);
        }
        StartTimer();
    }

    void Stop() {
        Clean();
        LogInfof(logger_, "job is done.");
        exit(0);
    }

    void SetLogger(Logger* logger) {
        logger_ = logger;
    }

private:
    int GetWhipIndex(const std::string& name) {
        int index = -1;
        for (CppStreamerInterface* whip : mediasoup_pusher_vec) {
            index++;
            if (!whip) {
                continue;
            }
            if (whip->StreamerName() == name) {
                return index;
            }
        }
        return -1;
    }

protected:
    virtual void OnReport(const std::string& name,
            const std::string& type,
            const std::string& value) override {
        LogWarnf(logger_, "report name:%s, type:%s, value:%s",
                name.c_str(), type.c_str(), value.c_str());
        if (type == "audio_produce") {
            if (value == "ready") {
                int index = GetWhipIndex(name);
                if (index < 0) {
                    LogErrorf(logger_, "fail to find whip by name:%s", name.c_str());
                } else {
                    LogWarnf(logger_, "whip streamer is ready, index:%d, name:%s",
                            index, name.c_str());
                }
                whip_ready_count_++;
            }
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

    void Clean() {
        if (tsdemux_streamer_) {
            delete tsdemux_streamer_;
            tsdemux_streamer_ = nullptr;
        }
        for (size_t i = 0; i < bench_count_; i++) {
            CppStreamerInterface* mediasoup_pusher = mediasoup_pusher_vec[i];
            if (mediasoup_pusher) {
                delete mediasoup_pusher;
                mediasoup_pusher_vec[i] = nullptr;
            }
        }
        uv_loop_close(loop_);
    }

    void AsyncClose() {
        async_.data = (void*)this;
        uv_async_send(&async_);
    }

    void OnWork() {
        int64_t start_ms = now_millisec();

        while(whip_ready_count_ < bench_count_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            int64_t now_ms = now_millisec();

            if (now_ms - start_ms > 15 * 1000) {
                LogErrorf(logger_, "mediasoup session is not ready and end the job");
                AsyncClose();
                return;
            }

            if (post_done_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                break;
            }
        }
        LogWarnf(logger_, "%d mediasoup session is ready", whip_ready_count_);

        FILE* file_p = fopen(src_ts_.c_str(), "r");
        if (!file_p) {
            LogErrorf(s_logger, "open mpegts file error:%s", src_ts_.c_str());
            AsyncClose();
            return;
        }
        uint8_t read_data[10*188];
        size_t read_n = 0;
        do {
            read_n = fread(read_data, 1, sizeof(read_data), file_p);
            if (read_n > 0) {
                InputTsData(read_data, read_n);
            }
            if (whip_ready_count_ == 0) {
                LogErrorf(logger_, "mediasoup pusher error and break mpegts reading");
                break;
            }
        } while (read_n > 0);
        fclose(file_p);
        LogInfof(logger_, "mpegts file read is over...");

        std::this_thread::sleep_for(std::chrono::milliseconds(3000));

        AsyncClose();
    }

private:
    std::string src_ts_;
    std::string base_url_;
    size_t bench_count_ = 1;
    uv_loop_t* loop_ = nullptr;
    uv_async_t async_;
    std::shared_ptr<std::thread> thread_ptr_;
    size_t whip_ready_count_ = 0;
    size_t whip_index_ = 0;
    bool post_done_ = false;

private:
    Logger* logger_ = nullptr;
    std::vector<CppStreamerInterface*> mediasoup_pusher_vec;
    CppStreamerInterface* tsdemux_streamer_    = nullptr;
};

void CloseCallback(uv_async_t *handle) {
    Mpegts2MediaSoupPushs* mediasoup_push = (Mpegts2MediaSoupPushs*)(handle->data);
    mediasoup_push->Stop();
}

int main(int argc, char** argv) {
    char input_ts_name[516];
    char output_url_name[516];
    char log_file[516];

    int opt = 0;
    bool input_ts_name_ready = false;
    bool output_url_name_ready = false;
    bool log_file_ready = false;
    int bench_count = 0;

    while ((opt = getopt(argc, argv, "i:o:l:n:h")) != -1) {
        switch (opt) {
            case 'i': strncpy(input_ts_name, optarg, sizeof(input_ts_name)); input_ts_name_ready = true; break;
            //./mediasoup_push_bench -i ~/movies/webrtc.ts -o "https://xxxxx.com:4443?roomId=200&userId=1000" -n 10
            case 'o': strncpy(output_url_name, optarg, sizeof(output_url_name)); output_url_name_ready = true; break;
            case 'n':
            {
                char count_sz[80];
                strncpy(count_sz, optarg, sizeof(count_sz));
                bench_count = atoi(count_sz);
                break;
            }
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-i input mpegts(h264+opus) file]\n\
    [-o whip url]\n\
    [-n bench count]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!input_ts_name_ready || !output_url_name_ready) {
        std::cout << "please input mpegts file/output whip url\r\n";
        return -1;
    }

    if (bench_count <= 0) {
        std::cout << "please input whip bench count.\r\n";
        return -1;
    }
    if (bench_count > BENCH_MAX) {
        std::cout << "bench count max is " << BENCH_MAX << ".\r\n";
        return -1;
    }

    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }
    s_logger->SetLevel(LOGGER_WARN_LEVEL);

    CppStreamerFactory::SetLogger(s_logger);
    CppStreamerFactory::SetLibPath("./output/lib");


    LogInfof(s_logger, "mpegts2whip bench is starting, input mpegts:%s, output whip bash url:%s, bench count:%d",
            input_ts_name, output_url_name, bench_count);
    uv_loop_t* loop = uv_default_loop();
 
    std::shared_ptr<Mpegts2MediaSoupPushs> mgr_ptr = std::make_shared<Mpegts2MediaSoupPushs>(loop, 
            input_ts_name, 
            output_url_name, 
            (size_t)bench_count);

    mgr_ptr->SetLogger(s_logger);

    if (mgr_ptr->MakeStreamers(loop) < 0) {
        LogErrorf(s_logger, "call mpegts to whip bench error");
        return -1;
    }
    LogInfof(s_logger, "mpegts to whip bench is starting......");
    mgr_ptr->Start();
    while (true) {
        uv_run(loop, UV_RUN_DEFAULT);
    }
    return 0;
}


