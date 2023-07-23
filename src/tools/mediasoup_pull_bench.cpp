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
static const size_t WHEPS_INTERVAL = 10;

class MediasoupPulls: public StreamerReport, public TimerInterface, public CppStreamerInterface
{
public:
    MediasoupPulls(uv_loop_t* loop,
            const std::string& src_url,
            size_t bench_count):TimerInterface(loop, 500)
                            , src_url_(src_url)
                            , bench_count_(bench_count)
    {
    }
    virtual ~MediasoupPulls()
    {
        StopTimer();
    }

public://CppStreamerInterface
    virtual std::string StreamerName() override {
        return "mediasoup_pull_bench";
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
        return 0;
    }
    virtual void StartNetwork(const std::string& url, void* loop_handle) override {
    }
    virtual void AddOption(const std::string& key, const std::string& value) override {
    }
    virtual void SetReporter(StreamerReport* reporter) override {
    }


//TimerInterface
protected:
    virtual void OnTimer() override {
        StartWheps();
    }

    std::string GetUrl(size_t index) {
        std::string url = src_url_;
        url += "&userId=1000";
        url += std::to_string(index);
        return url;
    }

public:
    int MakeStreamers(uv_loop_t* loop_handle) {
        loop_ = loop_handle;

        for (size_t i = 0; i < bench_count_; i++) {
            CppStreamerInterface* mediasoup_puller = CppStreamerFactory::MakeStreamer("mspull");
            mediasoup_puller->SetLogger(logger_);
            mediasoup_puller->SetReporter(this);
            mediasoup_puller->AddSinker(this);

            mediasoup_puller_vec.push_back(mediasoup_puller);
        }

        return 0;

    }

    void StartWheps() {
        if (post_done_) {
            return;
        }

        LogWarnf(logger_, "StartWheps  index:%lu", whep_index_);
        size_t i = 0;
        for (i = whep_index_; i < whep_index_ + WHEPS_INTERVAL;i++) {
            if (i >= bench_count_) {
                break;
            }
            std::string url = GetUrl(i);
            LogWarnf(logger_, "start network url:%s", url.c_str());
            try {
                mediasoup_puller_vec[i]->StartNetwork(url, loop_);
            } catch(CppStreamException& e) {
                LogErrorf(logger_, "mediasoup pull start network exception:%s", e.what());
            }
        }
        whep_index_ = i;
        if (whep_index_ >= bench_count_) {
            post_done_ = true;
        }
    }

    void Start() {
        StartTimer();
    }

    void Stop() {
        Clean();
        LogInfof(logger_, "job is done.");
        exit(0);
    }

private:
    int GetWhipIndex(const std::string& name) {
        int index = -1;
        for (CppStreamerInterface* whip : mediasoup_puller_vec) {
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
            }
        }
    }

protected:
    void Clean() {
        for (size_t i = 0; i < bench_count_; i++) {
            CppStreamerInterface* mediasoup_puller = mediasoup_puller_vec[i];
            if (mediasoup_puller) {
                delete mediasoup_puller;
                mediasoup_puller_vec[i] = nullptr;
            }
        }
        uv_loop_close(loop_);
    }

private:
    std::string src_url_;
    size_t bench_count_ = 1;
    uv_loop_t* loop_ = nullptr;
    size_t whep_index_ = 0;
    bool post_done_ = false;

private:
    Logger* logger_ = nullptr;
    std::vector<CppStreamerInterface*> mediasoup_puller_vec;
};
/*
 *./mediasoup_pull_bench -i "https://xxxxx.com.cn:4443?roomId=100&apid=7689e48c-09ae-48ca-8973-ad5de69de5e8&vpid=aadbbb0b-2e4e-4ed8-8bd6-22e3c50b9fc1" -l 1.log
 */
int main(int argc, char** argv) {
    char src_url_name[516];
    char log_file[516];

    int opt = 0;
    bool src_url_name_ready = false;
    bool log_file_ready = false;
    int bench_count = 0;

    while ((opt = getopt(argc, argv, "i:l:n:h")) != -1) {
        switch (opt) {
            case 'i': strncpy(src_url_name, optarg, sizeof(src_url_name)); src_url_name_ready = true; break;
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
                printf("Usage: %s [-i whep url]\n\
    [-n bench count]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!src_url_name_ready) {
        std::cout << "please input whep url\r\n";
        return -1;
    }

    if (bench_count <= 0) {
        std::cout << "please input whep bench count.\r\n";
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
    s_logger->SetLevel(LOGGER_INFO_LEVEL);

    CppStreamerFactory::SetLogger(s_logger);
    CppStreamerFactory::SetLibPath("./output/lib");


    LogInfof(s_logger, "mediasoup pull bench is starting, input mediasoup pull url:%s, bench count:%d",
            src_url_name, bench_count);
    uv_loop_t* loop = uv_default_loop();
 
    std::shared_ptr<MediasoupPulls> mgr_ptr = std::make_shared<MediasoupPulls>(loop, 
            src_url_name, 
            (size_t)bench_count);

    mgr_ptr->SetLogger(s_logger);

    if (mgr_ptr->MakeStreamers(loop) < 0) {
        LogErrorf(s_logger, "call mediasoup pull bench error");
        return -1;
    }
    LogInfof(s_logger, "mediasoup pull bench is starting......");
    mgr_ptr->Start();
    while (true) {
        uv_run(loop, UV_RUN_DEFAULT);
    }
    return 0;
}


