#include "format/wav/wav_demuxer.hpp"
#include "format/wav/wav_pub.hpp"
#include "utils/logger.hpp"

#include <unistd.h>
#include <iostream>
#include <vector>

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

class WavDataCallbackImpl : public WavDataCallbackI
{
public:
    WavDataCallbackImpl(const std::string& pcm_filename, int rate, Logger* logger):pcm_filename_(pcm_filename)
                                                                , rate_(rate)
                                                                , logger_(logger)
    {
        LogInfof(logger_, "pcm file:%s, rate:%d", pcm_filename_.c_str(), rate_);
    }
    virtual ~WavDataCallbackImpl()
    {
    }
public:
    virtual void OnWavPacketCallBack(const uint8_t* data, size_t len, const FormatChunk* format) override {
        LogDebugf(logger_, "wav packet channel:%d, sample rate:%d, data len:%lu",
            format->numChannels, format->sampleRate, len);
        if (rate_ == 0 || rate_ == format->sampleRate) {
            FILE* fp = fopen(pcm_filename_.c_str(), "a+");
            if (fp) {
                fwrite(data, 1, len, fp);
                fclose(fp);
            }
            return;
        }

    }

private:
    std::string pcm_filename_;
    int rate_ = 0;
    Logger* logger_ = nullptr;
};

int main(int argc, char** argv) {
    char input_wav_name[128];
    char output_pcm_name[128];
    char output_rate[128];
    char log_file[128];

    int opt = 0;
    int output_rate_value = 0;
    bool input_wav_name_ready = false;
    bool input_pcm_name_ready = false;
    bool log_file_ready = false;

    while ((opt = getopt(argc, argv, "i:o:r:l:h")) != -1) {
        switch (opt) {
            case 'i': strncpy(input_wav_name, optarg, sizeof(input_wav_name)); input_wav_name_ready = true; break;
            case 'o': strncpy(output_pcm_name, optarg, sizeof(output_pcm_name)); input_pcm_name_ready = true; break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'r': strncpy(output_rate, optarg, sizeof(output_rate)); output_rate_value = atoi(output_rate); break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-i wav file name]\n\
    [-o pcm file name]\n\
    [-r pcm sample rate]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }
    if (!input_wav_name_ready) {
        std::cout << "please input wav name\r\n";
        return -1;
    }
    if (!input_pcm_name_ready) {
        std::cout << "please input pcm name\r\n";
        return -1;
    }
    std::cout << "input ogg file:" << input_wav_name
        << ", output pcm file:" << output_pcm_name
        << ", log file:" << log_file << "\r\n";

    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }
    FileRead freader(input_wav_name);
    WavDataCallbackImpl cb(output_pcm_name, output_rate_value, s_logger);
    WavDemuxer wav_demuxer(&freader, &cb, s_logger);

    int ret = wav_demuxer.Demux();
    if (ret < 0) {
        std::cout << "wav demuxer error\r\n";
        return -1;
    }
    std::cout << "wav demuxer success\r\n";
    if (s_logger) {
        delete s_logger;
        s_logger = nullptr;
    }
    std::cout << "press any key to exit\r\n";
    getchar();

    return 0;
}
