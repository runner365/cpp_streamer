#include "rtmp_client_session.hpp"
#include "rtmp_control_handler.hpp"
#include "rtmp_pub.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "amf/amf0.hpp"

namespace cpp_streamer
{

RtmpClientSession::RtmpClientSession(uv_loop_t* loop,
        RtmpClientDataCallbackI* data_cb,
        RtmpClientCtrlCallbackI* ctrl_cb,
        Logger* logger):RtmpSessionBase(logger)
                        , conn_(loop, this, logger)
                        , data_cb_(data_cb)
                        , ctrl_cb_(ctrl_cb)
                        , hs_(this, logger)
                        , ctrl_handler_(this, logger)
                        , logger_(logger)
{
    LogInfof(logger_, "rtmp client session construct....");
}

RtmpClientSession::~RtmpClientSession() {
    LogInfof(logger_, "rtmp client session desctruct....");
    Close();
}

int RtmpClientSession::Start(const std::string& url, bool is_publish) {
    int ret = 0;

    ret = GetRtmpUrlInfo(url, host_, port_, req_.tcurl_, req_.app_, req_.stream_name_);
    if (ret != 0) {
        LogErrorf(logger_, "fail to get rtmp url:%s, return:%d", url.c_str(), ret);
        return ret;
    }
    req_.key_ = req_.app_;
    req_.key_ += "/";
    req_.key_ += req_.stream_name_;

    req_.publish_flag_ = is_publish;

    LogInfof(logger_, "get rtmp info host:%s, port:%d, tcurl:%s, app:%s, streamname:%s, key:%s, method:%s",
        host_.c_str(), port_, req_.tcurl_.c_str(), req_.app_.c_str(), req_.stream_name_.c_str(),
        req_.key_.c_str(), IsPublishDesc());
   
    this->conn_.Connect(this->host_, this->port_);
    
    return 0;
}

int RtmpClientSession::RtmpWrite(Media_Packet_Ptr pkt_ptr) {
    uint16_t csid;
    uint8_t  type_id;

    if (!conn_.IsConnect()) {
        LogInfof(logger_, "rtmp tcp connect is Closed...");
        return -1;
    }
    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        csid = 6;
        type_id = RTMP_MEDIA_PACKET_VIDEO;
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        csid = 4;
        type_id = RTMP_MEDIA_PACKET_AUDIO;
    } else if (pkt_ptr->av_type_ == MEDIA_METADATA_TYPE) {
        //csid = 6;
        //type_id = pkt_ptr->typeid_;
        LogInfof(logger_, "discard flv metadata");
        return RTMP_OK;
    } else {
        LogErrorf(logger_, "doesn't support av type:%d", (int)pkt_ptr->av_type_);
        return -1;
    }
   
    WriteDataByChunkStream(this, csid,
                    pkt_ptr->dts_, type_id,
                    pkt_ptr->streamid_, this->GetChunkSize(),
                    pkt_ptr->buffer_ptr_, logger_);

    return RTMP_OK;
}

int RtmpClientSession::RtmpSend(char* data, int len) {
    conn_.Send(data, len);
    if (client_phase_ == client_c0c1_phase) {
        ctrl_cb_->OnRtmpHandShakeSendC0C1(0, (uint8_t*)data, len);
    } else if (client_phase_ == client_s0s1s2_phase) {
        ctrl_cb_->OnRtmpHandShakeRecvS0S1S2(0, (uint8_t*)data, len);
    }
    return 0;
}

int RtmpClientSession::RtmpSend(std::shared_ptr<DataBuffer> data_ptr) {
    conn_.Send(data_ptr->Data(), data_ptr->DataLen());
    return 0;
}

DataBuffer* RtmpClientSession::GetRecvBuffer() {
    return &recv_buffer_;
}

void RtmpClientSession::Close() {
    conn_.Close();
}

bool RtmpClientSession::IsReady() {
    return client_phase_ == client_media_handle_phase;
}

void RtmpClientSession::TryRead() {
    conn_.AsyncRead();
    return;
}

void RtmpClientSession::OnConnect(int ret_code) {
    if (ret_code != 0) {
        LogErrorf(logger_, "rtmp tcp connect error:%d", ret_code);
        Close();
        return;
    }
    LogInfof(logger_, "rtmp client tcp connected...");
    client_phase_ = client_c0c1_phase;
    recv_buffer_.Reset();
    (void)hs_.SendC0C1();
    
    TryRead();
}

void RtmpClientSession::OnWrite(int ret_code, size_t sent_size) {
    if (ret_code != 0) {
        LogErrorf(logger_, "rtmp write error:%d", ret_code);
        Close();
        return;
    }
}

void RtmpClientSession::OnRead(int ret_code, const char* data, size_t data_size) {
    if (ret_code != 0) {
        LogErrorf(logger_, "rtmp on read error:%d", ret_code);
        //Close();
        ctrl_cb_->OnClose(ret_code);
        return;
    }

    recv_buffer_.AppendData(data, data_size);

    int ret = HandleMessage();
    if (ret < 0) {
        Close();
        ctrl_cb_->OnClose(ret);
        return;
    } else if (ret == RTMP_NEED_READ_MORE) {
        LogDebugf(logger_, "HandleMessage need read more...");
        TryRead();
        return;
    }
    LogInfof(logger_, "handle message unkown return:%d", ret);
}

int RtmpClientSession::HandleMessage() {
    int ret = 0;

    if (client_phase_ == client_c0c1_phase) {
        if (!recv_buffer_.Require(RtmpClientHandshake::s0s1s2_size)) {
            return RTMP_NEED_READ_MORE;
        }
        uint8_t* p = (uint8_t*)recv_buffer_.Data();
        ret = hs_.ParseS0S1S2(p, RtmpClientHandshake::s0s1s2_size);
        if (ret < 0) {
            LogErrorf(logger_, "rtmp handshake parse s0s1s3 error:%d", ret);
            ctrl_cb_->OnRtmpHandShakeRecvS0S1S2(ret, p, RtmpClientHandshake::s0s1s2_size);
            return ret;
        }
        ctrl_cb_->OnRtmpHandShakeRecvS0S1S2(ret, p, RtmpClientHandshake::s0s1s2_size);
        client_phase_ = client_s0s1s2_phase;
        ret = hs_.SendC2();
        if (ret < 0) {
            LogErrorf(logger_, "rtmp handshake send c2 error:%d", ret);
            return ret;
        }

        client_phase_ = client_connect_phase;
        //send rtmp connect
        ret = RtmpConnect();
        if (ret < 0) {
            LogErrorf(logger_, "rtmp connect error:%d", ret);
            return -1;
        }
        client_phase_ = client_connect_resp_phase;
        recv_buffer_.Reset();
        return RTMP_NEED_READ_MORE;
    }
    
    if (client_phase_ == client_connect_resp_phase) {
        ret = ReceiveRespMessage();
        if (ret < 0) {
            LogErrorf(logger_, "rtmp connect resp error:%d", ret);
            return -1;
        } else if (ret == RTMP_NEED_READ_MORE) {
            return RTMP_NEED_READ_MORE;
        } else if (ret == 0) {
            client_phase_ = client_create_stream_phase;
        } else {
            LogErrorf(logger_, "rtmp connect resp unkown return:%d", ret);
            return -1;
        }
    }
    
    if (client_phase_ == client_create_stream_phase) {
        //send create stream
        LogInfof(logger_, "client send create stream message...");
        ret = RtmpCreatestream();
        if (ret < 0) {
            LogErrorf(logger_, "rtmp create stream error:%d", ret);
            return ret;
        }
        recv_buffer_.Reset();
        client_phase_ = client_create_stream_resp_phase;
        return RTMP_NEED_READ_MORE;
    }

    if (client_phase_ == client_create_stream_resp_phase) {
        ret = ReceiveRespMessage();
        if (ret < 0) {
            LogErrorf(logger_, "rtmp receive resp message in the create stream response phrase error:%d", ret);
            return ret;
        } else if (ret == RTMP_NEED_READ_MORE) {
            return RTMP_NEED_READ_MORE;
        } else if (ret == 0) {
            if (req_.publish_flag_) {
                client_phase_ = client_create_publish_phase;
            } else {
                client_phase_ = client_create_play_phase;
            }
            LogInfof(logger_, "change to rtmp phase:%s", GetClientPhaseDesc(client_phase_));
            recv_buffer_.Reset();
        } else {
            LogErrorf(logger_, "rtmp connect unkown return:%d", ret);
            return -1;
        }
    }

    if (client_phase_ == client_create_play_phase) {
        ret = RtmpPlay();
        if (ret < 0) {
            LogErrorf(logger_, "rtmp play error:%d", ret);
            return ret;
        }
        recv_buffer_.Reset();
        client_phase_ = client_create_playpublish_resp_phase;
        return RTMP_NEED_READ_MORE;
    }

    if (client_phase_ == client_create_publish_phase) {
        ret = RtmpPublish();
        if (ret < 0) {
            LogErrorf(logger_, "rtmp publish error:%d", ret);
            return ret;
        }
        recv_buffer_.Reset();
        client_phase_ = client_create_playpublish_resp_phase;
        LogInfof(logger_, "client publish send done.");
        return RTMP_NEED_READ_MORE;
    }

    if (client_phase_ == client_create_playpublish_resp_phase) {
        ret = ReceiveRespMessage();
        if (ret < 0) {
            LogErrorf(logger_, "rtmp receive resp message in the play/publish response phrase error:%d", ret);
            return ret;
        } else if (ret == RTMP_NEED_READ_MORE) {
            return RTMP_NEED_READ_MORE;
        } else if (ret == 0) {
            if (req_.publish_flag_) {
                client_phase_ = client_media_handle_phase;
            } else {
                client_phase_ = client_media_handle_phase;
            }
            LogInfof(logger_, "change to rtmp phase:%s(%d)", GetClientPhaseDesc(client_phase_), client_phase_);
            recv_buffer_.Reset();
        } else {
            LogErrorf(logger_, "rtmp play/publish unkown return:%d", ret);
            return -1;
        }
    }

    LogInfof(logger_, "the client_phase_:%d", (int)client_phase_);
    if (client_phase_ == client_media_handle_phase) {
        LogDebugf(logger_, "rtmp client start media %s", IsPublishDesc());
        ret = ReceiveRespMessage();
        if (ret < 0) {
            return ret;
        }
        return RTMP_NEED_READ_MORE;
    }
    return ret;
}

int RtmpClientSession::ReceiveRespMessage() {
    CHUNK_STREAM_PTR cs_ptr;
    int ret = -1;

    while(true) {
        //receive fmt+csid | basic header | message header | data
        ret = ReadChunkStream(cs_ptr);
        if ((ret < RTMP_OK) || (ret == RTMP_NEED_READ_MORE)) {
            if (ret < RTMP_OK) {
                LogErrorf(logger_, "ReadChunkStream error:%d", ret);
            }
            return ret;
        }

        //check whether chunk stream is ready(data is full)
        if (!cs_ptr || !cs_ptr->IsReady()) {
            if (recv_buffer_.DataLen() > 0) {
                continue;
            }
            return RTMP_NEED_READ_MORE;
        }
        
        if ((cs_ptr->type_id_ >= RTMP_CONTROL_SET_CHUNK_SIZE) && (cs_ptr->type_id_ <= RTMP_CONTROL_SET_PEER_BANDWIDTH)) {
            ret = ctrl_handler_.HandleRtmpControlMessage(cs_ptr, false);
            if (ret < RTMP_OK) {
                LogInfof(logger_, "HandleRtmpControlMessage error:%d", ret);
                return ret;
            }
            reportCtrlMsg(cs_ptr);
            cs_ptr->Reset();
            if (recv_buffer_.DataLen() > 0) {
                continue;
            }
            break;
        } else if (cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_AMF0) {
            std::vector<AMF_ITERM*> amf_vec;
            RTMP_CLIENT_SESSION_PHASE current_phase = client_phase_;

            ret = ctrl_handler_.HandleServerCommandMessage(cs_ptr, amf_vec);
            if (ret < RTMP_OK) {
                for (auto iter : amf_vec) {
                    AMF_ITERM* temp = iter;
                    delete temp;
                }
                LogInfof(logger_, "HandleServerCommandMessageerror:%d", ret);
                return ret;
            }
            
            if (current_phase == client_connect_resp_phase) {
                reportConnectRespAmf(0, amf_vec);
            } else if (current_phase == client_create_stream_resp_phase) {
                reportCreateStreamRespAmf(0, amf_vec);
            } else if (current_phase == client_create_playpublish_resp_phase) {
                reportPlayPublishRespAmf(0, amf_vec);
                client_phase_ = client_media_handle_phase;
            }
            for (auto iter : amf_vec) {
                AMF_ITERM* temp = iter;
                delete temp;
            }
            cs_ptr->Reset();
            if (recv_buffer_.DataLen() > 0) {
                continue;
            }
            break;
        }  else if ((cs_ptr->type_id_ == RTMP_MEDIA_PACKET_VIDEO) || (cs_ptr->type_id_ == RTMP_MEDIA_PACKET_AUDIO)
                || (cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_META_DATA0) || (cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_META_DATA3)) {
            Media_Packet_Ptr pkt_ptr = GetMediaPacket(cs_ptr);
            if (!pkt_ptr || !(pkt_ptr->buffer_ptr_) || pkt_ptr->buffer_ptr_->DataLen() == 0) {
                return -1;
            }

            //handle video/audio
            //TODO: send media data to client callback....
            if (data_cb_) {
                data_cb_->OnMessage(RTMP_OK, pkt_ptr);
            }

            cs_ptr->Reset();
            if (recv_buffer_.DataLen() > 0) {
                continue;
            }
            ret = RTMP_NEED_READ_MORE;
        } else {
            LogWarnf(logger_, "rtmp client chunk typeid:%d is not supported.", cs_ptr->type_id_);
        }
    }
    return ret;
}

void RtmpClientSession::reportCtrlMsg(CHUNK_STREAM_PTR cs_ptr) {
    if (cs_ptr->type_id_ == RTMP_CONTROL_SET_CHUNK_SIZE) {
        if (cs_ptr->chunk_data_ptr_->DataLen() < 4) {
            LogErrorf(logger_, "set chunk size control message size error:%d", cs_ptr->chunk_data_ptr_->DataLen());
            ctrl_cb_->OnRtmpChunkSize(-1, 0);
            return;
        }
        uint32_t chunk_size = ByteStream::Read4Bytes((uint8_t*)cs_ptr->chunk_data_ptr_->Data());
        ctrl_cb_->OnRtmpChunkSize(0, chunk_size);
    } else if (cs_ptr->type_id_ == RTMP_CONTROL_WINDOW_ACK_SIZE) {
        if (cs_ptr->chunk_data_ptr_->DataLen() < 4) {
            LogErrorf(logger_, "window ack size control message size error:%d", cs_ptr->chunk_data_ptr_->DataLen());
            ctrl_cb_->OnRtmpWindowSize(-1, 0);
            return;
        }
        uint32_t remote_window_acksize = ByteStream::Read4Bytes((uint8_t*)cs_ptr->chunk_data_ptr_->Data());
        ctrl_cb_->OnRtmpWindowSize(0, remote_window_acksize);
    } else if (cs_ptr->type_id_ == RTMP_CONTROL_SET_PEER_BANDWIDTH) {
        if (cs_ptr->chunk_data_ptr_->DataLen() < 4) {
            LogErrorf(logger_, "update peer bandwidth error:%d", cs_ptr->chunk_data_ptr_->DataLen());
            ctrl_cb_->OnRtmpBandWidth(-1, 0);
            return;
        }
        uint32_t bandwidth = ByteStream::Read4Bytes((uint8_t*)cs_ptr->chunk_data_ptr_->Data());
        ctrl_cb_->OnRtmpBandWidth(0, bandwidth);
    } else if (cs_ptr->type_id_ == RTMP_CONTROL_ACK) {
        ctrl_cb_->OnRtmpCtrlAck();
    } else {
        LogErrorf(logger_, "don't handle rtmp control message, typeid:%d", cs_ptr->type_id_);
    }
}

void RtmpClientSession::getItemMap(AMF_ITERM* item, std::map<std::string, std::string>& items) {
    if (item->amf_type_ != AMF_DATA_TYPE_OBJECT) {
        return;
    }
    for (auto obj_item : item->amf_obj_) {
        std::string key = obj_item.first;
        std::string value;
        AMF_ITERM* amf_p = obj_item.second;

        if (amf_p->amf_type_ == AMF_DATA_TYPE_STRING) {
            value = amf_p->desc_str_;
        } else if (amf_p->amf_type_ == AMF_DATA_TYPE_NUMBER) {
            value = std::to_string(amf_p->number_);
        } else if (amf_p->amf_type_ == AMF_DATA_TYPE_BOOL) {
            value = amf_p->enable_ ? "enable" : "disable";
        }
        if (!key.empty() && !value.empty()) {
            items.insert(std::make_pair(key, value));
        } else {
            LogErrorf(logger_, "amf decode error, key:%s, value:%s",
                key.c_str(), value.c_str());
        }
    }
}

void RtmpClientSession::reportConnectRespAmf(int ret, const std::vector<AMF_ITERM*>& amf_vec) {
    std::string result;
    int64_t transaction_id = 0;
    std::map<std::string, std::string> items;

    if (ret < 0) {
        ctrl_cb_->OnRtmpConnectRecv(ret, result, transaction_id, items);
        return;
    }
    for (AMF_ITERM* item : amf_vec) {
        if (item->amf_type_ == AMF_DATA_TYPE_STRING) {
            result = item->desc_str_;
        }
        if (item->amf_type_ == AMF_DATA_TYPE_NUMBER) {
            transaction_id = (int64_t)item->number_;
        }
        if (item->amf_type_ == AMF_DATA_TYPE_OBJECT) {
            getItemMap(item, items);
        }
    }
    ctrl_cb_->OnRtmpConnectRecv(ret, result, transaction_id, items);
}

void RtmpClientSession::reportCreateStreamRespAmf(int ret, const std::vector<AMF_ITERM*>& amf_vec) {
    std::string result;
    int64_t transaction_id = 0;
    int64_t stream_id = 0;
    std::map<std::string, std::string> items;

    if (ret < 0) {
        ctrl_cb_->OnRtmpCreateStreamRecv(ret, result, transaction_id, stream_id, items);
        return;
    }
    int index = 0;
    for (AMF_ITERM* item : amf_vec) {
        if (item->amf_type_ == AMF_DATA_TYPE_STRING) {
            result = item->desc_str_;
        }
        if (item->amf_type_ == AMF_DATA_TYPE_NUMBER) {
            if (index == 0) {
                transaction_id = (int64_t)item->number_;
            } else if (index == 1) {
                stream_id = (int64_t)item->number_;
            }
        }
        if (item->amf_type_ == AMF_DATA_TYPE_OBJECT) {
            getItemMap(item, items);
        }
    }
    ctrl_cb_->OnRtmpCreateStreamRecv(ret, result,
        transaction_id, stream_id, items);
}

void RtmpClientSession::reportPlayPublishRespAmf(int ret, const std::vector<AMF_ITERM*>& amf_vec) {
    std::string status;
    int64_t transaction_id = 0;
    std::map<std::string, std::string> items;

    if (ret < 0) {
        ctrl_cb_->OnRtmpPlayPublishRecv(ret, status, transaction_id, items);
        return;
    }
    for (AMF_ITERM* item : amf_vec) {
        if (item->amf_type_ == AMF_DATA_TYPE_STRING) {
            status = item->desc_str_;
        }
        if (item->amf_type_ == AMF_DATA_TYPE_NUMBER) {
            transaction_id = (int64_t)item->number_;
        }
        if (item->amf_type_ == AMF_DATA_TYPE_OBJECT) {
            getItemMap(item, items);
        }
    }
    ctrl_cb_->OnRtmpPlayPublishRecv(ret, status, transaction_id, items);
}

int RtmpClientSession::RtmpConnect() {
    DataBuffer amf_buffer;
    std::map<std::string, std::string> items;

    AMF_Encoder::Encode(std::string("connect"), amf_buffer);
    double transid = (double)req_.transaction_id_;
    AMF_Encoder::Encode(transid, amf_buffer);
    items.insert(std::make_pair("connect", std::to_string(transid)));

    std::map<std::string, AMF_ITERM*> event_amf_obj;
    AMF_ITERM* app_item = new AMF_ITERM();
    app_item->SetAmfType(AMF_DATA_TYPE_STRING);
    app_item->desc_str_ = req_.app_;
    event_amf_obj.insert(std::make_pair("app", app_item));
    items.insert(std::make_pair("app", req_.app_));

    AMF_ITERM* type_item = new AMF_ITERM();
    type_item->SetAmfType(AMF_DATA_TYPE_STRING);
    type_item->desc_str_ = "nonprivate";
    event_amf_obj.insert(std::make_pair("type", type_item));
    items.insert(std::make_pair("type", type_item->desc_str_));

    AMF_ITERM* ver_item = new AMF_ITERM();
    ver_item->SetAmfType(AMF_DATA_TYPE_STRING);
    ver_item->desc_str_ = "FMS.3.1";
    event_amf_obj.insert(std::make_pair("flashVer", ver_item));
    items.insert(std::make_pair("flashVer", ver_item->desc_str_));

    AMF_ITERM* tcurl_item = new AMF_ITERM();
    tcurl_item->SetAmfType(AMF_DATA_TYPE_STRING);
    tcurl_item->desc_str_ = req_.tcurl_;
    event_amf_obj.insert(std::make_pair("tcUrl", tcurl_item));
    items.insert(std::make_pair("tcUrl", req_.tcurl_));

    ctrl_cb_->OnRtmpConnectSend(0, items);
    AMF_Encoder::Encode(event_amf_obj, amf_buffer);

    delete app_item;
    delete type_item;
    delete ver_item;
    delete tcurl_item;

    uint32_t stream_id = 0;
    int ret = WriteDataByChunkStream(this, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    stream_id, GetChunkSize(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return ret;
}

int RtmpClientSession::RtmpCreatestream() {
    DataBuffer amf_buffer;

    AMF_Encoder::Encode(std::string("createStream"), amf_buffer);
    double transid = (double)req_.transaction_id_;
    AMF_Encoder::Encode(transid, amf_buffer);
    AMF_Encoder::EncodeNull(amf_buffer);

    ctrl_cb_->OnRtmpCreateStreamSend(0, (int64_t)transid);

    uint32_t stream_id = 0;
    int ret = WriteDataByChunkStream(this, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    stream_id, GetChunkSize(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return ret;
}

int RtmpClientSession::RtmpPlay() {
    DataBuffer amf_buffer;

    AMF_Encoder::Encode(std::string("play"), amf_buffer);
    double transid = 0.0;
    AMF_Encoder::Encode(transid, amf_buffer);
    AMF_Encoder::EncodeNull(amf_buffer);
    AMF_Encoder::Encode(req_.stream_name_, amf_buffer);

    ctrl_cb_->OnRtmpPlayPublishSend("play", int64_t(transid), req_.stream_name_);

    uint32_t stream_id = 0;
    int ret = WriteDataByChunkStream(this, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    stream_id, GetChunkSize(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return ret;
}

int RtmpClientSession::RtmpPublish() {
    DataBuffer amf_buffer;

    AMF_Encoder::Encode(std::string("publish"), amf_buffer);
    double transid = 0.0;
    AMF_Encoder::Encode(transid, amf_buffer);
    AMF_Encoder::EncodeNull(amf_buffer);
    AMF_Encoder::Encode(req_.stream_name_, amf_buffer);
    AMF_Encoder::Encode(std::string("live"), amf_buffer);

    ctrl_cb_->OnRtmpPlayPublishSend("publish", int64_t(transid), req_.stream_name_);

    uint32_t stream_id = 0;
    int ret = WriteDataByChunkStream(this, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    stream_id, GetChunkSize(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return ret;
}

}
