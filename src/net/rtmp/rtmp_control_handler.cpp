#include "rtmp_control_handler.hpp"
#include "rtmp_session_base.hpp"

//#include "rtmp_server.hpp"
#include "data_buffer.hpp"

#include <map>

namespace cpp_streamer
{
uint32_t g_config_chunk_size = 4096;

RtmpControlHandler::RtmpControlHandler(RtmpSessionBase* session,
        Logger* logger):session_(session)
                        , logger_(logger)
{
    LogInfof(logger, "RtmpControlHandler construct...");
}

RtmpControlHandler::~RtmpControlHandler()
{
}

int RtmpControlHandler::HandleServerCommandMessage(CHUNK_STREAM_PTR cs_ptr, std::vector<AMF_ITERM*>& amf_vec) {
    uint8_t* data = (uint8_t*)cs_ptr->chunk_data_ptr_->Data();
    int len = (int)cs_ptr->chunk_data_ptr_->DataLen();

    while (len > 0) {
        AMF_ITERM* amf_item = new AMF_ITERM();
        AMF_Decoder::Decode(data, len, *amf_item);

        amf_vec.push_back(amf_item);
    }

    if (amf_vec.size() < 1) {
        LogErrorf(logger_, "amf vector count error:%lu", amf_vec.size());
        return -1;
    }

    RTMP_CLIENT_SESSION_PHASE next_phase = session_->client_phase_;

    for (auto item : amf_vec) {
        if (item->GetAmfType() == AMF_DATA_TYPE_STRING) {
            if ((session_->client_phase_ == client_connect_phase) ||
                (session_->client_phase_ == client_connect_resp_phase) ||
                (session_->client_phase_ == client_create_stream_phase) ||
                (session_->client_phase_ == client_create_stream_resp_phase)) {
                if ((item->desc_str_ != "_result") && (item->desc_str_ != "onBWDone")) {
                    LogErrorf(logger_, "rtmp client connect error: %s", item->desc_str_.c_str());
                    return -1;
                }
                if ((session_->client_phase_ == client_connect_phase) || 
                    (session_->client_phase_ == client_connect_resp_phase)) {
                    if (item->desc_str_ == "_result") {
                        //LogInfof(logger_, "rtmp client change connect to create stream.");
                        next_phase = client_create_stream_phase;
                    }
                }
                if ((session_->client_phase_ == client_create_stream_phase) ||
                    (session_->client_phase_ == client_create_stream_resp_phase)){
                    if (item->desc_str_ == "_result") {
                        LogDebugf(logger_, "rtmp client change create stream to %s", session_->IsPublishDesc());
                        if (session_->IsPublish()) {
                            next_phase = client_create_publish_phase;
                        } else {
                            next_phase = client_create_play_phase;
                        }
                    }
                }
            } else if ((session_->client_phase_ == client_create_publish_phase) ||
                    (session_->client_phase_ == client_create_play_phase)) {
                if ((item->desc_str_ != "_result") && (item->desc_str_ != "onStatus") && (item->desc_str_ != "onBWDone")) {
                    LogErrorf(logger_, "rtmp client %s return %s", session_->IsPublishDesc(), item->desc_str_.c_str());
                    return -1;
                }
            }
        } else if (item->GetAmfType() == AMF_DATA_TYPE_NUMBER) {
            LogDebugf(logger_, "rtmp client phase:[%s], amf number:%f",
                GetClientPhaseDesc(session_->client_phase_), item->number_);
        } else if (item->GetAmfType() == AMF_DATA_TYPE_OBJECT) {
            LogDebugf(logger_, "rtmp client phase:[%s], amf object", GetClientPhaseDesc(session_->client_phase_));
            auto iter = item->amf_obj_.find("code");
            if (iter != item->amf_obj_.end()) {
                AMF_ITERM* obj_item = iter->second;
                if (obj_item->GetAmfType() == AMF_DATA_TYPE_STRING) {
                    LogDebugf(logger_, "client phase[%s] %s", GetClientPhaseDesc(session_->client_phase_),
                            obj_item->desc_str_.c_str());
                    if (session_->client_phase_ == client_connect_phase) {
                        if (obj_item->desc_str_ != "NetConnection.Connect.Success") {
                            LogErrorf(logger_, "rtmp client connect return %s", obj_item->desc_str_.c_str());
                            return -1;
                        }
                    } else if ((session_->client_phase_ == client_create_publish_phase) ||
                            (session_->client_phase_ == client_create_play_phase)) {
                        if ((obj_item->desc_str_ != "NetStream.Publish.Start") && (obj_item->desc_str_ != "NetStream.Play.Start")) {
                            LogErrorf(logger_, "rtmp client [%s] return %s",GetClientPhaseDesc(session_->client_phase_),
                                obj_item->desc_str_.c_str());
                            return -1;
                        }
                    }   
                }
            }
        }
    }

    if (session_->client_phase_ != next_phase) {
        session_->client_phase_ = next_phase;
    }
    return 0;
}

int RtmpControlHandler::HandleClientCommandMessage(CHUNK_STREAM_PTR cs_ptr, std::vector<AMF_ITERM*>& amf_vec) {
    int ret = 0;
    uint8_t* data = (uint8_t*)cs_ptr->chunk_data_ptr_->Data();
    int len = (int)cs_ptr->chunk_data_ptr_->DataLen();

    while (len > 0) {
        AMF_ITERM* amf_item = new AMF_ITERM();
        AMF_Decoder::Decode(data, len, *amf_item);

        amf_vec.push_back(amf_item);
    }

    if (amf_vec.size() < 1) {
        LogErrorf(logger_, "amf vector count error:%lu", amf_vec.size());
        return -1;
    }

    AMF_ITERM* item = amf_vec[0];
    std::string cmd_type;

    if (item->GetAmfType() != AMF_DATA_TYPE_STRING) {
        LogErrorf(logger_, "first amf type error:%d", (int)item->GetAmfType());
        return -1;
    }

    cmd_type = item->desc_str_;

    if (cmd_type == CMD_Connect) {
        ret = HandleRtmpConnectCommand(cs_ptr->msg_stream_id_, amf_vec);
    } else if (cmd_type == CMD_CreateStream) {
        ret = HandleRtmpCreatestreamCommand(cs_ptr->msg_stream_id_, amf_vec);
    } else if (cmd_type == CMD_Publish) {
        ret = HandleRtmpPublishCommand(cs_ptr->msg_stream_id_, amf_vec);
    } else if (cmd_type == CMD_Play) {
        ret = HandleRtmpPlayCommand(cs_ptr->msg_stream_id_, amf_vec);
    }

    if (ret == RTMP_OK) {
        if (session_->req_.is_ready_ && !session_->req_.publish_flag_) {
            return ret;
        }
        ret = RTMP_NEED_READ_MORE;
    }
    return ret;
}

int RtmpControlHandler::HandleRtmpConnectCommand(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec) {
    if (amf_vec.size() < 3) {
        LogErrorf(logger_, "rtmp connect amf vector count error:%lu", amf_vec.size());
        return -1;
    }

    double transactionId = 0;
    for (int index = 1; index < (int)amf_vec.size(); index++) {
        AMF_ITERM* item = amf_vec[index];
        switch (item->GetAmfType())
        {
            case AMF_DATA_TYPE_NUMBER:
            {
                transactionId = item->number_;
                LogDebugf(logger_, "rtmp transactionId:%f", transactionId);
                break;
            }
            case AMF_DATA_TYPE_OBJECT:
            {
                std::map<std::string, AMF_ITERM*>::iterator iter = item->amf_obj_.find("app");
                if (iter != item->amf_obj_.end()) {
                    AMF_ITERM* item = iter->second;
                    if (item->GetAmfType() != AMF_DATA_TYPE_STRING) {
                        LogErrorf(logger_, "app type is not string:%d", (int)item->GetAmfType());
                        return -1;
                    }
                    session_->req_.app_ = item->desc_str_;
                }
                iter = item->amf_obj_.find("tcUrl");
                if (iter != item->amf_obj_.end()) {
                    AMF_ITERM* item = iter->second;
                    if (item->GetAmfType() != AMF_DATA_TYPE_STRING) {
                        LogErrorf(logger_, "tcUrl type is not string:%d", (int)item->GetAmfType());
                        return -1;
                    }
                    session_->req_.tcurl_ = item->desc_str_;
                }
                iter = item->amf_obj_.find("flashVer");
                if (iter != item->amf_obj_.end()) {
                    AMF_ITERM* item = iter->second;
                    if (item->GetAmfType() != AMF_DATA_TYPE_STRING) {
                        LogErrorf(logger_, "flash ver type is not string:%d", (int)item->GetAmfType());
                        return -1;
                    }
                    session_->req_.flash_ver_ = item->desc_str_;
                }

                break;
            }
            default:
                break;
        }
    }
    return SendRtmpConnectResp(session_->stream_id_);
}

int RtmpControlHandler::HandleRtmpCreatestreamCommand(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec) {
    if (amf_vec.size() < 3) {
        LogErrorf(logger_, "rtmp create stream amf vector count error:%lu", amf_vec.size());
        return -1;
    }
    double transactionId = 0;
    
    session_->req_.stream_id_ = stream_id;
    for (int index = 1; index < (int)amf_vec.size(); index++) {
        AMF_ITERM* item = amf_vec[index];
        switch (item->GetAmfType())
        {
            case AMF_DATA_TYPE_NUMBER:
            {
                //LogInfof(logger_, "rtmp create stream transaction id:%f", item->number_);
                transactionId = item->number_;
                session_->req_.transaction_id_ = (int64_t)transactionId;
                break;
            }
            case AMF_DATA_TYPE_OBJECT:
            {
                //LogInfof(logger_, "rtmp create stream object:");
                item->DumpAmf();
                break;
            }
            default:
                break;
        }
    }
    return SendRtmpCreateStreamResp(transactionId);
}

int RtmpControlHandler::HandleRtmpPlayCommand(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec) {
    if (amf_vec.size() < 3) {
        LogErrorf(logger_, "rtmp play amf vector count error:%lu", amf_vec.size());
        return -1;
    }
    double transactionId = 0;
    std::string stream_name;
    for (int index = 1; index < (int)amf_vec.size(); index++) {
        AMF_ITERM* item = amf_vec[index];
        switch (item->GetAmfType())
        {
            case AMF_DATA_TYPE_NUMBER:
            {
                //LogInfof(logger_, "rtmp play transaction id:%f", item->number_);
                transactionId = item->number_;
                session_->req_.transaction_id_ = (int64_t)transactionId;
                break;
            }
            case AMF_DATA_TYPE_STRING:
            {
                //LogInfof(logger_, "rtmp play string:%s", item->desc_str_.c_str());
                if (stream_name.empty()) {
                    stream_name = item->desc_str_;
                }
                break;
            }
            case AMF_DATA_TYPE_OBJECT:
            {
                //LogInfof(logger_, "rtmp play object:");
                item->DumpAmf();
                break;
            }
            default:
                break;
        }
    }
    session_->req_.stream_name_  = stream_name;
    session_->req_.publish_flag_ = false;
    session_->req_.is_ready_     = true;
    session_->req_.key_ = session_->req_.app_;
    session_->req_.key_ += "/";
    session_->req_.key_ += stream_name;
    
    //LogInfof(logger_, "rtmp play is ready, key:%s", session_->req_.key_.c_str());
    return SendRtmpPlayResp();
}

int RtmpControlHandler::HandleRtmpPublishCommand(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec) {
    if (amf_vec.size() < 3) {
        LogErrorf(logger_, "rtmp publish amf vector count error:%lu", amf_vec.size());
        return -1;
    }
    double transactionId = 0;
    std::string stream_name;
    for (int index = 1; index < (int)amf_vec.size(); index++) {
        AMF_ITERM* item = amf_vec[index];
        switch (item->GetAmfType())
        {
            case AMF_DATA_TYPE_NUMBER:
            {
                //LogInfof(logger_, "rtmp publish transaction id:%f", item->number_);
                transactionId = item->number_;
                session_->req_.transaction_id_ = (int64_t)transactionId;
                break;
            }
            case AMF_DATA_TYPE_STRING:
            {
                if (stream_name.empty()) {
                    stream_name = item->desc_str_;
                }
                break;
            }
            case AMF_DATA_TYPE_OBJECT:
            {
                //LogInfof(logger_, "rtmp publish object:");
                item->DumpAmf();
                break;
            }
            default:
                break;
        }
    }
    session_->req_.stream_name_  = stream_name;
    session_->req_.publish_flag_ = true;
    session_->req_.is_ready_     = true;
    session_->req_.key_ = session_->req_.app_;
    session_->req_.key_ += "/";
    session_->req_.key_ += stream_name;
    return SendRtmpPublishResp();
}

int RtmpControlHandler::SendSetChunksize(uint32_t chunk_size) {
    ChunkStream set_chunk_size_cs(session_, 0, 3, session_->GetChunkSize(), logger_);
    set_chunk_size_cs.GenControlMessage(RTMP_CONTROL_SET_CHUNK_SIZE, 4, chunk_size);

    //LogInfof(logger_, "send ctrl message chunk size:%u", chunk_size);
    session_->RtmpSend(set_chunk_size_cs.chunk_all_ptr_);
    return 0;
}

int RtmpControlHandler::SendRtmpConnectResp(uint32_t stream_id) {
    DataBuffer amf_buffer;
    ChunkStream win_size_cs(session_, 0, 3, session_->GetChunkSize(), logger_);
    ChunkStream peer_bw_cs(session_, 0, 3, session_->GetChunkSize(), logger_);
    ChunkStream set_chunk_size_cs(session_, 0, 3, session_->GetChunkSize(), logger_);

    win_size_cs.GenControlMessage(RTMP_CONTROL_WINDOW_ACK_SIZE, 4, 2500000);
    peer_bw_cs.GenControlMessage(RTMP_CONTROL_SET_PEER_BANDWIDTH, 5, 2500000);
    set_chunk_size_cs.GenControlMessage(RTMP_CONTROL_SET_CHUNK_SIZE, 4, g_config_chunk_size);
    LogInfof(logger_, "SendRtmpConnectResp chunk size:%u", session_->GetChunkSize());
    session_->SetChunkSize(g_config_chunk_size);

    session_->RtmpSend(win_size_cs.chunk_all_ptr_);
    //LogInfof(logger_, "rtmp send windows size");
    session_->RtmpSend(peer_bw_cs.chunk_all_ptr_);
    //LogInfof(logger_, "rtmp send peer bandwidth");
    session_->RtmpSend(set_chunk_size_cs.chunk_all_ptr_);
    //LogInfof(logger_, "rtmp send set chunk size");

    //Encode resp amf
    std::map<std::string, AMF_ITERM*> resp_amf_obj;
    AMF_ITERM* fms_ver_item = new AMF_ITERM();
    fms_ver_item->SetAmfType(AMF_DATA_TYPE_STRING);
    fms_ver_item->desc_str_ = "FMS/3,0,1,123";
    resp_amf_obj.insert(std::make_pair("fmsVer", fms_ver_item));

    AMF_ITERM* capability_item = new AMF_ITERM();
    capability_item->SetAmfType(AMF_DATA_TYPE_NUMBER);
    capability_item->number_ = 31.0;
    resp_amf_obj.insert(std::make_pair("capabilities", capability_item));
    std::map<std::string, AMF_ITERM*> event_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->SetAmfType(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    event_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->SetAmfType(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetConnection.Connect.Success";
    event_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->SetAmfType(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Connection succeeded.";
    event_amf_obj.insert(std::make_pair("description", desc_item));

    std::string result_str = "_result";
    AMF_Encoder::Encode(result_str, amf_buffer);
    double transaction_id = 1.0;
    AMF_Encoder::Encode(transaction_id, amf_buffer);
    AMF_Encoder::Encode(resp_amf_obj, amf_buffer);
    AMF_Encoder::Encode(event_amf_obj, amf_buffer);
    delete fms_ver_item;
    delete capability_item;
    delete level_item;
    delete code_item;
    delete desc_item;

    //LogInfof(logger_, "rtmp connection resp, data len:%lu", amf_buffer.DataLen());
    int ret = WriteDataByChunkStream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    stream_id, session_->GetChunkSize(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    //LogInfof(logger_, "rtmp connect done, tcurl:%s", session_->req_.tcurl_.c_str());
    session_->server_phase_ = create_stream_phase;
    return RTMP_OK;
}

int RtmpControlHandler::SendRtmpCreateStreamResp(double transaction_id) {
    DataBuffer amf_buffer;
    std::string result_str = "_result";
    double stream_id = (double)session_->stream_id_;

    AMF_Encoder::Encode(result_str, amf_buffer);
    AMF_Encoder::Encode(transaction_id, amf_buffer);
    AMF_Encoder::EncodeNull(amf_buffer);
    AMF_Encoder::Encode(stream_id, amf_buffer);

    int ret = WriteDataByChunkStream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->GetChunkSize(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    //LogInfof(logger_, "rtmp create stream done tcurl:%s", session_->req_.tcurl_.c_str());
    session_->server_phase_ = create_publish_play_phase;
    return RTMP_OK;
}

int RtmpControlHandler::SendRtmpPlayResp() {
    uint32_t chunk_size = 6;

    //send rtmp control recorded
    ChunkStream ctrl_recorded(session_, 0, 3, chunk_size, logger_);
    ctrl_recorded.GenSetRecordedMessage();
    session_->RtmpSend(ctrl_recorded.chunk_all_ptr_);

    //send rtmp control begin
    ChunkStream ctrl_begin(session_, 0, 3, chunk_size, logger_);
    ctrl_begin.GenSetBeginMessage();
    session_->RtmpSend(ctrl_begin.chunk_all_ptr_);

    SendRtmpPlayResetResp();
    SendRtmpPlayStartResp();
    SendRtmpPlayDataResp();
    SendRtmpPlayNotifyResp();

    //LogInfof(logger_, "rtmp play start key:%s", session_->req_.key_.c_str());

    return RTMP_OK;
}

int RtmpControlHandler::SendRtmpPlayResetResp() {
    DataBuffer amf_buffer;
    double transaction_id = 0.0;

    std::string result_str = "onStatus";
    AMF_Encoder::Encode(result_str, amf_buffer);
    AMF_Encoder::Encode(transaction_id, amf_buffer);
    AMF_Encoder::EncodeNull(amf_buffer);

    std::map<std::string, AMF_ITERM*> resp_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->SetAmfType(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    resp_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->SetAmfType(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetStream.Play.Reset";
    resp_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->SetAmfType(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Playing and resetting stream.";
    resp_amf_obj.insert(std::make_pair("description", desc_item));
    AMF_Encoder::Encode(resp_amf_obj, amf_buffer);

    delete level_item;
    delete code_item;
    delete desc_item;

    int ret = WriteDataByChunkStream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->GetChunkSize(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return RTMP_OK;
}

int RtmpControlHandler::SendRtmpPlayStartResp() {
    DataBuffer amf_buffer;
    double transaction_id = 0.0;

    std::string result_str = "onStatus";
    AMF_Encoder::Encode(result_str, amf_buffer);
    AMF_Encoder::Encode(transaction_id, amf_buffer);
    AMF_Encoder::EncodeNull(amf_buffer);

    std::map<std::string, AMF_ITERM*> resp_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->SetAmfType(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    resp_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->SetAmfType(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetStream.Play.Start";
    resp_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->SetAmfType(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Started playing stream.";
    resp_amf_obj.insert(std::make_pair("description", desc_item));
    AMF_Encoder::Encode(resp_amf_obj, amf_buffer);

    delete level_item;
    delete code_item;
    delete desc_item;

    int ret = WriteDataByChunkStream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->GetChunkSize(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return RTMP_OK;
}

int RtmpControlHandler::SendRtmpPlayDataResp() {
    DataBuffer amf_buffer;
    double transaction_id = 0.0;

    std::string result_str = "onStatus";
    AMF_Encoder::Encode(result_str, amf_buffer);
    AMF_Encoder::Encode(transaction_id, amf_buffer);
    AMF_Encoder::EncodeNull(amf_buffer);

    std::map<std::string, AMF_ITERM*> resp_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->SetAmfType(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    resp_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->SetAmfType(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetStream.Data.Start";
    resp_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->SetAmfType(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Started playing stream.";
    resp_amf_obj.insert(std::make_pair("description", desc_item));
    AMF_Encoder::Encode(resp_amf_obj, amf_buffer);

    delete level_item;
    delete code_item;
    delete desc_item;

    int ret = WriteDataByChunkStream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->GetChunkSize(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return RTMP_OK;
}

int RtmpControlHandler::SendRtmpPlayNotifyResp() {
    DataBuffer amf_buffer;
    double transaction_id = 0.0;

    std::string result_str = "onStatus";
    AMF_Encoder::Encode(result_str, amf_buffer);
    AMF_Encoder::Encode(transaction_id, amf_buffer);
    AMF_Encoder::EncodeNull(amf_buffer);

    std::map<std::string, AMF_ITERM*> resp_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->SetAmfType(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    resp_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->SetAmfType(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetStream.Play.PublishNotify";
    resp_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->SetAmfType(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Started playing notify.";
    resp_amf_obj.insert(std::make_pair("description", desc_item));
    AMF_Encoder::Encode(resp_amf_obj, amf_buffer);

    delete level_item;
    delete code_item;
    delete desc_item;

    int ret = WriteDataByChunkStream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->GetChunkSize(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return RTMP_OK;
}

int RtmpControlHandler::SendRtmpPublishResp() {
    DataBuffer amf_buffer;
    double transaction_id = 0.0;

    std::string result_str = "onStatus";
    AMF_Encoder::Encode(result_str, amf_buffer);
    AMF_Encoder::Encode(transaction_id, amf_buffer);
    AMF_Encoder::EncodeNull(amf_buffer);

    std::map<std::string, AMF_ITERM*> resp_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->SetAmfType(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    resp_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->SetAmfType(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetStream.Publish.Start";
    resp_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->SetAmfType(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Start publising.";
    resp_amf_obj.insert(std::make_pair("description", desc_item));
    AMF_Encoder::Encode(resp_amf_obj, amf_buffer);

    delete level_item;
    delete code_item;
    delete desc_item;

    int ret = WriteDataByChunkStream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->GetChunkSize(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    //LogInfof(logger_, "rtmp publish start key:%s", session_->req_.key_.c_str());
    session_->server_phase_ = media_handle_phase;
    return RTMP_OK;
}

int RtmpControlHandler::SendRtmpAck(uint32_t size) {
    int ret = 0;
    session_->ack_received_ += size;

    if (session_->ack_received_ > session_->remote_window_acksize_) {
        ChunkStream ack(session_, 0, 3, session_->GetChunkSize(), logger_);
        ret += ack.GenControlMessage(RTMP_CONTROL_ACK, 4, session_->GetChunkSize());
        session_->RtmpSend(ack.chunk_all_ptr_);
        session_->ack_received_ = 0;
    }
    return ret;
}

int RtmpControlHandler::HandleRtmpControlMessage(CHUNK_STREAM_PTR cs_ptr, bool is_server) {
    if (cs_ptr->type_id_ == RTMP_CONTROL_SET_CHUNK_SIZE) {
        if (cs_ptr->chunk_data_ptr_->DataLen() < 4) {
            LogErrorf(logger_, "set chunk size control message size error:%d", cs_ptr->chunk_data_ptr_->DataLen());
            return -1;
        }

        session_->SetChunkSize(ByteStream::Read4Bytes((uint8_t*)cs_ptr->chunk_data_ptr_->Data()));
        LogInfof(logger_, "update chunk size:%u, is_server:%s",
                session_->GetChunkSize(), is_server ? "true" : "false");
        //if (is_server) {
            SendSetChunksize(session_->GetChunkSize());
        //}
    } else if (cs_ptr->type_id_ == RTMP_CONTROL_WINDOW_ACK_SIZE) {
        if (cs_ptr->chunk_data_ptr_->DataLen() < 4) {
            LogErrorf(logger_, "window ack size control message size error:%d", cs_ptr->chunk_data_ptr_->DataLen());
            return -1;
        }
        session_->remote_window_acksize_ = ByteStream::Read4Bytes((uint8_t*)cs_ptr->chunk_data_ptr_->Data());
        //LogInfof(logger_, "update remote window ack size:%u", session_->remote_window_acksize_);
    } else if (cs_ptr->type_id_ == RTMP_CONTROL_SET_PEER_BANDWIDTH) {
        //LogInfof(logger_, "update peer bandwidth:%u", ByteStream::Read4Bytes((uint8_t*)cs_ptr->chunk_data_ptr_->Data()));
    } else if (cs_ptr->type_id_ == RTMP_CONTROL_ACK) {
        //LogInfof(logger_, "receive rtmp control ack...");
    } else {
        //LogInfof(logger_, "don't handle rtmp control message:%d", cs_ptr->type_id_);
    }
    return RTMP_OK;
}

}
