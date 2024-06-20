#include "cpp_streamer_interface.hpp"
#include "cpp_streamer_factory.hpp"
#include "logger.hpp"
#include "media_packet.hpp"
#include "format/mp4/mp4_box.hpp"
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

class Mp4DumpMgr : public CppStreamerInterface, public StreamerReport
{
public:
    Mp4DumpMgr(const std::string& filename)
    {
        file_reader_ = new Mp4FileReader(filename);
    }
    virtual ~Mp4DumpMgr()
    {
        if (mp4_demux_streamer_) {
            delete mp4_demux_streamer_;
            mp4_demux_streamer_ = nullptr;
        }
        if (file_reader_) {
            delete file_reader_;
            file_reader_ = nullptr;
        }
    }

public:
    int MakeStreamers() {
        mp4_demux_streamer_ = CppStreamerFactory::MakeStreamer("mp4demux");
        if (!mp4_demux_streamer_) {
            LogErrorf(logger_, "make streamer mp4demux error");
            return -1;
        }
        LogInfof(logger_, "make mp4 demux streamer:%p, name:%s", mp4_demux_streamer_, mp4_demux_streamer_->StreamerName().c_str());
        mp4_demux_streamer_->SetLogger(logger_);
        mp4_demux_streamer_->SetReporter(this);
        mp4_demux_streamer_->AddSinker(this);
        mp4_demux_streamer_->AddOption("box_detail", "true");
        return 0;
    }

    void Start() {
        if (!mp4_demux_streamer_) {
            LogErrorf(logger_, "mp4 demux streamer is not ready");
            return;
        }
        Media_Packet_Ptr pkt_ptr = std::make_shared<Media_Packet>();
        pkt_ptr->io_reader_ = file_reader_;

        mp4_demux_streamer_->SourceData(pkt_ptr);
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
        return "mp4dump";
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
        if (pkt_ptr->av_type_ == MEDIA_MOVBOX_TYPE) {
            if (pkt_ptr->box_type_ == "ftyp") {
                FtypBox* box = (FtypBox*)(pkt_ptr->box_);
                std::cout << "ftyp box dump:" << box->Dump() << "\r\n";
                LogInfof(logger_, "ftyp box:%s", box->Dump().c_str());
            } else if (pkt_ptr->box_type_ == "moov") {
                MoovBox* box = (MoovBox*)(pkt_ptr->box_);
                std::cout << "moov box dump:" << box->Dump() << "\r\n";
                LogInfof(logger_, "moov box:%s", box->Dump().c_str());
            } else if (pkt_ptr->box_type_ == "free") {
                FreeBox* box = (FreeBox*)(pkt_ptr->box_);
                std::cout << "free box dump:" << box->Dump() << "\r\n";
                LogInfof(logger_, "free box:%s", box->Dump().c_str());
            } else if (pkt_ptr->box_type_ == "mdat") {
                MdatBox* box = (MdatBox*)(pkt_ptr->box_);
                std::cout << "mdat box dump:" << box->Dump() << "\r\n";
                LogInfof(logger_, "mdat box:%s", box->Dump().c_str());
            } else {
                Mp4BoxBase* box = (Mp4BoxBase*)(pkt_ptr->box_);
                std::cout << "box dump:" << box->Dump() << "\r\n";
                LogInfof(logger_, "unknown box:%s", box->Dump().c_str());
            }
        } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
            if (pkt_ptr->is_seq_hdr_) {
                LogInfof(logger_, "audio sequence header:%s", pkt_ptr->Dump(true).c_str());
            } else {
                LogInfof(logger_, "audio data:%s", pkt_ptr->Dump().c_str());
            }
        } else if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
            if (pkt_ptr->is_seq_hdr_) {
                LogInfof(logger_, "video sequence header:%s", pkt_ptr->Dump(true).c_str());
            } else {
                LogInfof(logger_, "video data:%s", pkt_ptr->Dump(false).c_str());

                if (pkt_ptr->codec_type_ == MEDIA_CODEC_H265) {
                    uint8_t* p = (uint8_t*)pkt_ptr->buffer_ptr_->Data();
                    int len = pkt_ptr->buffer_ptr_->DataLen();
                    std::vector<std::shared_ptr<DataBuffer>> nalus;

                    bool ret = AnnexB2Nalus(p, len, nalus);
                    if (ret) {
                        for (std::shared_ptr<DataBuffer> nalu : nalus) {
                            p = (uint8_t*)nalu->Data();
                            int pos = GetNaluTypePos(p);

                            if (pos > 0) {
                                Hevc_Header header;
                                GetHevcHeader(p + pos, header);
                                LogInfof(logger_, "hevc header:%s", HevcHeaderDump(header).c_str());
                            }
                        }
                    }
                }
            }
        } else {
            assert(0);
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
    void handleVideoSequnceData(const std::vector<uint8_t> sequence_data) {
    }
    void handleAudioSequnceData(const std::vector<uint8_t> sequence_data) {
    }
private:
    Logger* logger_ = nullptr;
    CppStreamerInterface* mp4_demux_streamer_ = nullptr;
    Mp4FileReader* file_reader_ = nullptr;
};

int main(int argc, char** argv) {
    char input_mp4_name[128];
    char log_file[128];

    int opt = 0;
    bool input_mp4_name_ready = false;
    bool log_file_ready = false;

    while ((opt = getopt(argc, argv, "i:l:h")) != -1) {
        switch (opt) {
            case 'i': strncpy(input_mp4_name, optarg, sizeof(input_mp4_name)); input_mp4_name_ready = true; break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-i mp4 file name]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!input_mp4_name_ready) {
        std::cout << "please input mp4 name\r\n";
        return -1;
    }

    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }

    CppStreamerFactory::SetLogger(s_logger);
    CppStreamerFactory::SetLibPath("./output/lib");

    LogInfof(s_logger, "mp4 dump streamer manager is starting, input filename:%s", input_mp4_name);

    auto streamer_mgr_ptr = std::make_shared<Mp4DumpMgr>(input_mp4_name);

    streamer_mgr_ptr->SetLogger(s_logger);
    if (streamer_mgr_ptr->MakeStreamers() < 0) {
        LogErrorf(s_logger, "call make streamer error");
        return -1;
    }
    streamer_mgr_ptr->Start();

    LogInfof(s_logger, "mp4 dump done");

    streamer_mgr_ptr = nullptr;
    CppStreamerFactory::ReleaseAll();
    
    delete s_logger;


    return 0;
}