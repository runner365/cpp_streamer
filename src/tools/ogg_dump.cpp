#include "ogg_demuxer.hpp"
#include "utils/logger.hpp"

#include <unistd.h>

using namespace cpp_streamer;

static Logger* s_logger = nullptr;

class FileRead : public IoReaderI
{
public:
    FileRead(const std::string& filename):filename_(filename)
    {
        file_ = fopen(filename.c_str(), "r");
        if (!file_) {
            CSM_THROW_ERROR("read file exception");
        }
    }
    virtual ~FileRead()
    {
        fclose(file_);
    }
public:
    virtual int IoRead(uint8_t*& data, size_t len) override {
        return fread(data, 1, len, file_);
    }

private:
    std::string filename_;
    FILE* file_ = nullptr;
};

class OpusDataCallbackImpl : public OpusDataCallbackI
{
public:
    OpusDataCallbackImpl(Logger* logger):logger_(logger)
    {
    }
    ~OpusDataCallbackImpl()
    {
    }
public:
    virtual void OnOpusPacketCallBack(int channel, int sample_rate, const uint8_t* data, size_t len, int64_t dts) override {
        LogInfof(logger_, "opus packet channel:%d, sample rate:%d, data len:%lu, dts:%ld",
            channel, sample_rate, len, dts);
    }

private:
    Logger* logger_ = nullptr;
};

int main(int argc, char** argv) {
    char input_ogg_name[128];
    char log_file[128];

    int opt = 0;
    bool input_ogg_name_ready = false;
    bool log_file_ready = false;

    while ((opt = getopt(argc, argv, "i:l:h")) != -1) {
        switch (opt) {
            case 'i': strncpy(input_ogg_name, optarg, sizeof(input_ogg_name)); input_ogg_name_ready = true; break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-i ogg file name]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }
    if (!input_ogg_name_ready) {
        std::cout << "please input ogg name\r\n";
        return -1;
    }

    std::cout << "input ogg file:" << input_ogg_name << ", log file:" << log_file << "\r\n";
    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }
    FileRead freader(input_ogg_name);
    OpusDataCallbackImpl cb(s_logger);
    OggDemuxer demuxer(&freader, &cb, s_logger);

    demuxer.Demux();
    getchar();

    delete s_logger;
    return 0;
}