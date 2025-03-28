#include "ogg_demuxer.hpp"
#include "ogg_pub.hpp"

#include <memory>
#include <utility>

using json = nlohmann::json;

namespace cpp_streamer
{

OggDemuxer::OggDemuxer(IoReaderI* io_reader, OpusDataCallbackI* cb, Logger* logger):io_reader_(io_reader)
                                                            , cb_(cb)
                                                            , logger_(logger)
{
}

OggDemuxer::~OggDemuxer()
{
}


int OggDemuxer::Demux() {
    std::vector<uint8_t> header_buffer(OGG_PAGE_HEADER_SIZE);
    uint8_t* p = nullptr;
    size_t page_index = 0;

    while (true) {
        p = &header_buffer[0];
        int n = io_reader_->IoRead(p, OGG_PAGE_HEADER_SIZE);
        if (n < OGG_PAGE_HEADER_SIZE) {
            LogInfof(logger_, "ogg read eof.");
            break;
        }
        std::shared_ptr<OggPage> page_ptr = std::make_shared<OggPage>();

        page_ptr->header_ = OGG_PAGE_HEADER::Parse(p, OGG_PAGE_HEADER_SIZE);
        if (page_ptr->header_ == nullptr) {
            LogErrorf(logger_, "parse ogg header error");
            break;
        }

        //read segment lens
        page_ptr->segs_size_.resize(page_ptr->header_->seg_num);
        p = &page_ptr->segs_size_[0];
        n = io_reader_->IoRead(p, page_ptr->header_->seg_num);
        if (n < page_ptr->header_->seg_num) {
            LogInfof(logger_, "ogg read segment number eof.");
            break;
        }
        page_index++;
        LogInfof(logger_, "ogg page:%d, dump:%s", page_index, page_ptr->Dump().c_str());

        //read segments data
        bool err = false;
        for (size_t i = 0; i < page_ptr->segs_size_.size(); i++) {
            int len = page_ptr->segs_size_[i];
            std::vector<uint8_t> temp_buffer(len);
            uint8_t* temp_data = &temp_buffer[0];

            n = io_reader_->IoRead(temp_data, len);
            if (n < len) {
                LogInfof(logger_, "ogg read segment sample eof.");
                err = true;
                break;
            }
            // LogInfoData(logger_, temp_data, len, "ogg seg data");

            if (OpusExtraHandler::IsExtraData(temp_data, len)) {
                int ret = extra_data_handler_.DemuxExtraData(temp_data, len);
                if (ret == 0) {
                    std::string dump = extra_data_handler_.DumpExtraData();
                    LogInfof(logger_, "opus extra data:%s", dump.c_str());
                } else {
                    LogErrorf(logger_, "demux extra data error");
                }
            }
            std::shared_ptr<DataBuffer> data_ptr = std::make_shared<DataBuffer>(len);
            data_ptr->AppendData((char*)temp_data, len);
            page_ptr->seg_buffer_vec_.push_back(data_ptr);
        }

        if (err) {
            break;
        }
        for (auto d_ptr : page_ptr->seg_buffer_vec_) {
            cb_->OnOpusPacketCallBack(extra_data_handler_.channel_,
                                    extra_data_handler_.samperate_,
                                    (uint8_t*)d_ptr->Data(),
                                    d_ptr->DataLen(),
                                    dts_);
            dts_ += dts_interval_;
        }
    }

    return 0;
}

void OggDemuxer::SetDtsInterval(int64_t dts_interval) {
    dts_interval_ = dts_interval;
}

}