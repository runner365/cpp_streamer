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

class Mp4FileReader : public IoReadInterface
{
public:
    Mp4FileReader(const std::string& filename):filename_(filename)
    {
        file_ = fopen(filename_.c_str(), "r");
        if (!file_) {
            CSM_THROW_ERROR("mp4 read file error:%s", filename.c_str());
        }
    }
    virtual ~Mp4FileReader()
    {
        if (file_) {
            fclose(file_);
            file_ = nullptr;
        }
    }
public:
    virtual int Read(size_t offset, uint8_t* data_buffer, size_t data_buffer_len) override {        
        int ret = -1;

        if (!file_) {
            return ret;
        }

        fseek(file_, offset, 0);
        ret = fread(data_buffer, 1, data_buffer_len, file_);

        return ret;
    }

private:
    FILE* file_ = nullptr;
    std::string filename_;
};

class Mp4toFlvStreamerMgr : public CppStreamerInterface, public StreamerReport
{
public:
    Mp4toFlvStreamerMgr(const std::string& in_filename, const std::string& output_filename):filename_(output_filename)
    {
        reader_ = new Mp4FileReader(in_filename);
    }
    virtual ~Mp4toFlvStreamerMgr()
    {
        if (mp4_demux_streamer_) {
            delete mp4_demux_streamer_;
            mp4_demux_streamer_ = nullptr;
        }
        if (flv_mux_streamer_) {
            delete flv_mux_streamer_;
            flv_mux_streamer_ = nullptr;
        }
        if (reader_) {
            delete reader_;
            reader_ = nullptr;
        }
    }

public:
    int MakeStreamers() {
        mp4_demux_streamer_ = CppStreamerFactory::MakeStreamer("mp4demux");
        if (!mp4_demux_streamer_) {
            LogErrorf(logger_, "make streamer mp4 demux error");
            return -1;
        }
        LogInfof(logger_, "make mp4 demux streamer:%p, name:%s", mp4_demux_streamer_, mp4_demux_streamer_->StreamerName().c_str());
        mp4_demux_streamer_->SetLogger(logger_);
        mp4_demux_streamer_->SetReporter(this);
 
        flv_mux_streamer_ = CppStreamerFactory::MakeStreamer("flvmux");
        if (!flv_mux_streamer_) {
            LogErrorf(logger_, "make streamer flvmux error");
            return -1;
        }
        LogInfof(logger_, "make flv mux streamer:%p, name:%s", flv_mux_streamer_, flv_mux_streamer_->StreamerName().c_str());
        flv_mux_streamer_->SetLogger(logger_);
        flv_mux_streamer_->AddSinker(this);
        flv_mux_streamer_->SetReporter(this);
        mp4_demux_streamer_->AddSinker(flv_mux_streamer_);
        return 0;
    }

    int InputMp4Data(uint8_t* data, size_t data_len) {
        if (!mp4_demux_streamer_) {
            LogErrorf(logger_, "flv demux streamer is not ready");
            return -1;
        }
        Media_Packet_Ptr pkt_ptr = std::make_shared<Media_Packet>(data_len);
        pkt_ptr->buffer_ptr_->AppendData((char*)data, data_len);
        pkt_ptr->io_reader_ = reader_;
        mp4_demux_streamer_->SourceData(pkt_ptr);
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
        return "mp4toflv_manager";
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
        FILE* file_p = fopen(filename_.c_str(), "ab+");
        if (file_p) {
            fwrite(pkt_ptr->buffer_ptr_->Data(), 1, pkt_ptr->buffer_ptr_->DataLen(), file_p);
            fclose(file_p);
        }
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
    std::string filename_;
    CppStreamerInterface* mp4_demux_streamer_ = nullptr;
    CppStreamerInterface* flv_mux_streamer_ = nullptr;
    Mp4FileReader* reader_ = nullptr;
};

int main(int argc, char** argv) {
    char input_mp4_name[128];
    char output_flv_name[128];
    char log_file[128];

    int opt = 0;
    bool input_mp4_name_ready = false;
    bool output_flv_name_ready = false;
    bool log_file_ready = false;

    while ((opt = getopt(argc, argv, "i:o:l:h")) != -1) {
        switch (opt) {
            case 'i': strncpy(input_mp4_name, optarg, sizeof(input_mp4_name)); input_mp4_name_ready = true; break;
            case 'o': strncpy(output_flv_name, optarg, sizeof(output_flv_name)); output_flv_name_ready = true; break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-i mp4 file name]\n\
    [-o flv file name]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!input_mp4_name_ready || !output_flv_name_ready) {
        std::cout << "please input/output file name\r\n";
        return -1;
    }

    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }

    CppStreamerFactory::SetLogger(s_logger);
    CppStreamerFactory::SetLibPath("./output/lib");

    LogInfof(s_logger, "mp4 to flv streamer manager is starting, input filename:%s, output filename:%s",
            input_mp4_name, output_flv_name);
    auto streamer_mgr_ptr = std::make_shared<Mp4toFlvStreamerMgr>(
        std::string(input_mp4_name),
        std::string(output_flv_name));

    streamer_mgr_ptr->SetLogger(s_logger);
    if (streamer_mgr_ptr->MakeStreamers() < 0) {
        LogErrorf(s_logger, "call GenFlvDemuxStreamer error");
        return -1;
    }
    FILE* file_p = fopen(input_mp4_name, "r");
    if (!file_p) {
        LogErrorf(s_logger, "open flv file error:%s", input_mp4_name);
        return -1;
    }
    uint8_t read_data[2048];
    size_t read_n = 0;
    do {
        read_n = fread(read_data, 1, sizeof(read_data), file_p);
        if (read_n > 0) {
            streamer_mgr_ptr->InputMp4Data(read_data, read_n);
        }
    } while (read_n > 0);
    fclose(file_p);

    //std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    LogInfof(s_logger, "mp4 to flv done");

    streamer_mgr_ptr = nullptr;
    CppStreamerFactory::ReleaseAll();
    
    getchar();
    delete s_logger;

    return 0;
}
