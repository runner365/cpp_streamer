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
static const int BENCH_MAX = 1000;
static const size_t WHIPS_INTERVAL = 10;

static size_t done_count = 0;

void CloseCallback(uv_async_t *handle);

class Mpegts2Whips: public StreamerReport, public TimerInterface
{
public:
    Mpegts2Whips(uv_loop_t* loop,
            const std::string& src_ts, 
            const std::string& output_url,
            size_t stream_index,
            size_t bench_count):TimerInterface(loop, 500)
                            , src_ts_(src_ts)
                            , base_url_(output_url)
                            , whip_index_(stream_index)
                            , base_index_(stream_index)
                            , bench_count_(bench_count)
    {
    }
    virtual ~Mpegts2Whips()
    {
        StopTimer();
        worker_thread_ptr_->join();
        worker_thread_ptr_ = nullptr;
        uv_thread_ptr_->join();
        uv_thread_ptr_ = nullptr;
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
        tsdemux_streamer_->SetLogger(logger_);
        tsdemux_streamer_->AddOption("re", "true");
        tsdemux_streamer_->SetReporter(this);

        for (size_t i = 0; i < bench_count_; i++) {
            CppStreamerInterface* whip_streamer = CppStreamerFactory::MakeStreamer("whip");
            whip_streamer->SetLogger(logger_);
            whip_streamer->SetReporter(this);
            tsdemux_streamer_->AddSinker(whip_streamer);

            whips_.push_back(whip_streamer);
        }

        return 0;

    }

    void StartWhips() {
        if (post_done_) {
            return;
        }

        LogWarnf(logger_, "StartWhips whip index:%lu, bench_count:%lu",
                whip_index_, bench_count_);
        size_t i = 0;
        for (i = whip_index_; i < whip_index_ + WHIPS_INTERVAL;i++) {
            if (i >= base_index_ + bench_count_) {
                break;
            }
            std::string url = GetUrl(i);
            LogWarnf(logger_, "start network url:%s", url.c_str());
            try {
                size_t index = i - base_index_;
                whips_[index]->StartNetwork(url, loop_);
            } catch(CppStreamException& e) {
                LogErrorf(logger_, "whip start network exception:%s", e.what());
            }
        }
        whip_index_ = i;
        if (whip_index_ >= base_index_ + bench_count_) {
            post_done_ = true;
        }
    }

    void Start() {
        running_ = true;

        if (!worker_thread_ptr_) {
            worker_thread_ptr_ = std::make_shared<std::thread>(&Mpegts2Whips::OnWork, this);
        }

        if (!uv_thread_ptr_) {
            uv_thread_ptr_ = std::make_shared<std::thread>(&Mpegts2Whips::OnUvWork, this);
        }
        StartTimer();
    }

    void Stop() {
        running_ = false;
        Clean();
        LogInfof(logger_, "job is done.");

        done_count++;
    }

    void SetLogger(Logger* logger) {
        logger_ = logger;
    }

private:
    void OnUvWork() {
        while (running_) {
            uv_run(loop_, UV_RUN_DEFAULT);
        }
    }

    int GetWhipIndex(const std::string& name) {
        int index = -1;
        for (CppStreamerInterface* whip : whips_) {
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
        if ((log_count_++ % 4) == 0) {
            LogWarnf(logger_, "report name:%s, type:%s, value:%s",
                    name.c_str(), type.c_str(), value.c_str());
        }
        if (type == "dtls") {
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
            CppStreamerInterface* whip_streamer = whips_[i];
            if (whip_streamer) {
                delete whip_streamer;
                whips_[i] = nullptr;
            }
        }
        //uv_loop_close(loop_);
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
                LogErrorf(logger_, "whip session is not ready and end the job");
                AsyncClose();
                return;
            }

            if (post_done_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                break;
            }
        }
        LogWarnf(logger_, "%d whip session is ready", whip_ready_count_);

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
                LogErrorf(logger_, "whip error and break mpegts reading");
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
    size_t whip_index_ = 0;
    size_t base_index_ = 0;
    size_t bench_count_ = 1;
    bool running_ = false;
    uv_loop_t* loop_ = nullptr;
    uv_async_t async_;
    std::shared_ptr<std::thread> worker_thread_ptr_;
    std::shared_ptr<std::thread> uv_thread_ptr_;
    size_t whip_ready_count_ = 0;
    bool post_done_ = false;

private:
    Logger* logger_ = nullptr;
    std::vector<CppStreamerInterface*> whips_;
    CppStreamerInterface* tsdemux_streamer_    = nullptr;

private:
    int64_t log_count_ = 0;
};

void CloseCallback(uv_async_t *handle) {
    Mpegts2Whips* whip = (Mpegts2Whips*)(handle->data);
    whip->Stop();
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
            /*eg: http://10.0.24.12:1985/rtc/v1/whip/?app=live&stream=1000*/
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
    size_t group_count = bench_count / 100;
    group_count += (bench_count % 100 == 0) ? 0 : 1;

    std::cout << "bench_count/100:" << bench_count/100 << "\r\n";
    std::cout << "bench_count%100:" << bench_count%100 << "\r\n";

    std::vector<std::shared_ptr<Mpegts2Whips>> mgr_vec;

    std::cout << "group count:" << group_count << "\r\n";
    std::cout << "bench count:" << bench_count << "\r\n";

    for (size_t i = 0; i < group_count; i++) {
        size_t base_index = i * 100;
        size_t count = (i != (group_count - 1)) ? 100 : (bench_count % 100);
        uv_loop_t *loop = (uv_loop_t*)malloc(sizeof(uv_loop_t));
        uv_loop_init(loop); 

        std::shared_ptr<Mpegts2Whips> mgr_ptr = std::make_shared<Mpegts2Whips>(loop, 
                input_ts_name, 
                output_url_name, 
                base_index,
                count);
        mgr_ptr->SetLogger(s_logger);

        if (mgr_ptr->MakeStreamers(loop) < 0) {
            LogErrorf(s_logger, "call mpegts to whip bench error");
            return -1;
        }

        mgr_vec.push_back(mgr_ptr);
    }

    LogWarnf(s_logger, "webrtc streamer manager vector size:%lu", mgr_vec.size());

    int index = 0;
    for (auto mgr_ptr : mgr_vec) {
        LogInfof(s_logger, "mpegts to whip bench is starting index:%d", index++);
        mgr_ptr->Start();
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    } while(done_count < group_count);

    std::cout << "group count:" << group_count << " are all done.\r\n";

    exit(0);
    return 0;
}


