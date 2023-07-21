#include "ms_pull.hpp"
#include "mediasoup_pub.hpp"
#include "uuid.hpp"
#include <uv.h>
#include <sstream>

#include <iostream>
#include <stdio.h>

void* make_mspull_streamer() {
    cpp_streamer::MsPull* ms = new cpp_streamer::MsPull();
    return ms;
}

void destroy_mspull_streamer(void* streamer) {
    cpp_streamer::MsPull* ms = (cpp_streamer::MsPull*)streamer;
    delete ms;
}

using json = nlohmann::json;

namespace cpp_streamer
{
#define MEDIASOUP_PULL_NAME "mspull"

MsPull::MsPull()
{
    ByteCrypto::Init();
    name_ = MEDIASOUP_PULL_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
}

MsPull::~MsPull()
{
    LogInfof(logger_, "destruct mediasoup pu");
    ReleaseHttpClient(hc_req_);
    ReleaseHttpClient(hc_transport_);
    ReleaseHttpClient(hc_video_consume_);
    ReleaseHttpClient(hc_audio_consume_);
    if (pc_) {
        delete pc_;
        pc_ = nullptr;
    }
}

std::string MsPull::StreamerName() {
    return name_;
}

void MsPull::SetLogger(Logger* logger) {
    logger_ = logger;
}

int MsPull::AddSinker(CppStreamerInterface* sinker) {
    if (!sinker) {
        return sinkers_.size();
    }
    sinkers_[sinker->StreamerName()] = sinker;
    return sinkers_.size();
}

int MsPull::RemoveSinker(const std::string& name) {
    return sinkers_.erase(name);
}

int MsPull::SourceData(Media_Packet_Ptr pkt_ptr) {
    return 0;
}

void MsPull::StartNetwork(const std::string& url, void* loop_handle) {
    if (pc_) {
        delete pc_;
        pc_ = nullptr;
    }

    GetHostInfoByUrl(url, host_, port_, 
            roomId_, userId_,
            video_produce_id_,
            audio_produce_id_);
    LogInfof(logger_, "http post host:%s, port:%d, roomId:%s, userId:%s, video produceId:%s, audio produceId:%s",
            host_.c_str(), port_, roomId_.c_str(), userId_.c_str(), 
            video_produce_id_.c_str(), audio_produce_id_.c_str());
    loop_ = (uv_loop_t*)loop_handle;

    pc_ = new PeerConnection((uv_loop_t*)loop_handle, logger_, this);

    BroadCasterRequest();
    return;
}

void MsPull::AddOption(const std::string& key, const std::string& value) {
    auto iter = options_.find(key);
    if (iter == options_.end()) {
        std::stringstream ss;
        ss << "the option key:" << key << " does not exist";
        throw CppStreamException(ss.str().c_str());
    }
    options_[key] = value;
    LogInfof(logger_, "set mediaspu broadcaster options key:%s, value:%s", key.c_str(), value.c_str());
}

void MsPull::SetReporter(StreamerReport* reporter) {
    report_ = reporter;
}

bool MsPull::GetHostInfoByUrl(const std::string& url, std::string& host, uint16_t& port, std::string& roomId, std::string& userId, 
        std::string& video_produce_id,
        std::string& audio_produce_id) {
    const std::string https_scheme("https://");
    std::string hostUrl(url);

    //https://xxxx.com:8080/?roomId=1000&userId=10000&vpid=xxxxxx&apid=xxxxxxx
    RemoveSubfix(hostUrl, "/");

    size_t pos = hostUrl.find(https_scheme);
    if (pos != 0) {
        LogErrorf(logger_, "host url:%s error, when try to find https scheme", url.c_str());
        return false;
    }

    hostUrl = hostUrl.substr(https_scheme.length());//xxxx.com:8080/?roomId=100&userId=1000

    pos = hostUrl.find("/");
    if (pos != std::string::npos) {
        std::string hostname = hostUrl.substr(0, pos);//xxxx.com:8080
        pos = hostname.find(":");
        if (pos == std::string::npos) {
            LogErrorf(logger_, "host url:%s error, when try to find ':'", url.c_str());
            return false;
        }
        host = hostname.substr(0, pos);
        port = atoi(hostname.substr(pos + 1).c_str());
    } else {
        pos = hostUrl.find("?");
        if (pos == std::string::npos) {
            LogErrorf(logger_, "host url:%s error, when try to find '?'", url.c_str());
            return false;
        }
        std::string hostname = hostUrl.substr(0, pos);//xxxx.com:8080
        pos = hostname.find(":");
        if (pos == std::string::npos) {
            LogErrorf(logger_, "host url:%s error, when try to find ':' after find '?'", url.c_str());
            return false;
        }
        host = hostname.substr(0, pos);
        port = atoi(hostname.substr(pos + 1).c_str());
    }

    pos = hostUrl.find("?");
    if (pos == std::string::npos) {
        LogErrorf(logger_, "host url:%s error, fail to find '?'", url.c_str());
        return false;
    }

    std::string params = hostUrl.substr(pos + 1);
    std::vector<std::string> params_vec;

    StringSplit(params, "&", params_vec);

    for (auto& param : params_vec) {
        pos = param.find("=");
        if (pos == std::string::npos) {
            continue;
        }
        std::string name = param.substr(0, pos);
        std::string value = param.substr(pos + 1);

        if (name == "roomId") {
            roomId = value;
        } else if (name == "userId") {
            userId = value;
        } else if (name == "vpid") {
            video_produce_id = value;
        } else if (name == "apid") {
            audio_produce_id = value;
        }
    }
    return true;
}

void MsPull::BroadCasterRequest() {
    std::map<std::string, std::string> headers;
    hc_req_ = new HttpClient(loop_, host_, port_, this, logger_, true);

    std::stringstream subpath;
    ///rooms/:roomId/broadcasters
    subpath << "/rooms/" << roomId_ << "/broadcasters";

    headers["content-type"] = "application/json";
    //'content-type: application/json'
    auto req_json    = json::object();
    auto device_json = json::object();
    auto rtp_json    = json::object();

    GetRtpCapabilities(rtp_json);

    req_json["id"]              = userId_;
    req_json["displayName"]     = userId_;
    device_json["name"]         = "cpp_streamer";
    req_json["device"]          = device_json;
    req_json["rtpCapabilities"] = rtp_json;

    state_ = BROADCASTER_REQUEST;

    LogInfof(logger_, "http post subpath:%s, json data:%s",
            subpath.str().c_str(), req_json.dump().c_str());
    hc_req_->Post(subpath.str(), headers, req_json.dump());
}

void MsPull::OnHttpRead(int ret, std::shared_ptr<HttpClientResponse> resp_ptr) {
    if (ret < 0) {
        LogInfof(logger_, "http request ret:%d", ret);
        return;
    }
    LogInfof(logger_, "http step:%d", state_);
    if (state_ == BROADCASTER_REQUEST) {
        HandleBroadCasterResponse(resp_ptr);
    } else if (state_ == BROADCASTER_TRANSPORT) {
        HandleTransportResponse(resp_ptr);
    } else if (state_ == BROADCASTER_TRANSPORT_CONNECT) {
        HandleTransportConnectResponse(resp_ptr);
    } else if (state_ == BROADCASTER_VIDEO_CONSUME) {
        HandleVideoConsumeResponse(resp_ptr);
    } else if (state_ == BROADCASTER_AUDIO_CONSUME) {
        HandleAudioConsumeResponse(resp_ptr);
    }

}

void MsPull::HandleBroadCasterResponse(std::shared_ptr<HttpClientResponse> resp_ptr) {
    if (resp_ptr->status_code_ != 200) {
        std::string data_str(resp_ptr->data_.Data(), resp_ptr->data_.DataLen());
        LogErrorf(logger_, "http broadcaster response state:%d, resp:%s", 
                resp_ptr->status_code_, data_str.c_str());
        return;
    }
    Report("broadcaster", "ready");
    std::string data_str(resp_ptr->data_.Data(), resp_ptr->data_.DataLen());

    LogInfof(logger_, "http broad caster response:%s", data_str.c_str());

    TransportRequest();
}

void MsPull::HandleTransportResponse(std::shared_ptr<HttpClientResponse> resp_ptr) {
    std::string data_str(resp_ptr->data_.Data(), resp_ptr->data_.DataLen());
    json data_json = json::parse(data_str);
    LogInfof(logger_, "http broadcaster response state:%d, resp:%s", 
            resp_ptr->status_code_, data_str.c_str());

    if (resp_ptr->status_code_ != 200) {
        LogErrorf(logger_, "http broadcaster response state:%d error", resp_ptr->status_code_);
        return;
    }
    Report("transport_send", "ready");
    transport_id_ = data_json["id"];

    auto iceParameters_json = data_json["iceParameters"];
    ice_pwd_ = iceParameters_json["password"];
    ice_usr_frag_ = iceParameters_json["usernameFragment"];

    auto iceCandidates_array = data_json["iceCandidates"];
    for (auto& item : iceCandidates_array) {
        candidate_ip_    = item["ip"];
        candidate_port_  = item["port"];
        candidate_proto_ = item["protocol"];
    }

    auto dtlsParameters_json = data_json["dtlsParameters"];
    auto fingerprints_array = dtlsParameters_json["fingerprints"];

    for (auto& item : fingerprints_array) {
        std::string algorithm = item["algorithm"];
        if (algorithm != "sha-256") {
            continue;
        }
        alg_value_ = item["value"];
    }

    LogInfof(logger_, "webrtc transport ice_pwd:%s, user_fragment:%s, candidate_ip:%s, candidate_port:%d, finger_print:%s",
            ice_pwd_.c_str(), ice_usr_frag_.c_str(), candidate_ip_.c_str(),
            candidate_port_, alg_value_.c_str());
    pc_->SetRemoteIcePwd(ice_pwd_);
    pc_->SetRemoteIceUserFrag(ice_usr_frag_);
    pc_->SetRemoteUdpAddress(candidate_ip_, candidate_port_);
    pc_->SetFingerPrintsSha256(alg_value_);

    pc_->CreateOfferSdp(SEND_ONLY);
    pc_->UpdatePcState(PC_SDP_DONE_STATE);

    TransportConnectRequest();

    return;
}

void MsPull::HandleTransportConnectResponse(std::shared_ptr<HttpClientResponse> resp_ptr) {
    std::string data_str(resp_ptr->data_.Data(), resp_ptr->data_.DataLen());
    LogInfof(logger_, "http transport connect response state:%d, resp:%s", 
            resp_ptr->status_code_, data_str.c_str());
    if (resp_ptr->status_code_ != 200) {
        LogErrorf(logger_, "http transport connect response state:%d error", resp_ptr->status_code_);
        return;
    }
    Report("transport_connect", "ready");
    pc_->StartTimer();
}

void MsPull::ParseVideoConsume(const std::string& data) {
    auto ret_json = json::parse(data);

    video_consume_id_ = ret_json["id"];

    std::string prd_id = ret_json["producerId"];
    if (prd_id != video_produce_id_) {
        CSM_THROW_ERROR("video consume return produceId(%s) != video produceId(%s)", prd_id.c_str(), video_produce_id_.c_str());
    }

    auto rtp_parameter_json = ret_json["rtpParameters"];
    auto encodings_json = ret_json["encodings"];

    uint32_t ssrc = 0;
    uint32_t rtx_ssrc = 0;
    bool found_ssrc = false;
    for(auto & encoding_json : encodings_json) {
        auto ssrc_it = encoding_json.find("ssrc");
        if (ssrc_it != encoding_json.end() && ssrc_it->is_number()) {
            ssrc = (uint32_t)ssrc_it->get<int>();
            found_ssrc = true;

            auto rtx_it = encoding_json.find("rtx");
            if (rtx_it != encoding_json.end() && rtx_it->is_object()) {
                auto rtx_ssrc_it = (*rtx_it).find("ssrc");
                if (rtx_ssrc_it != (*rtx_it).end() && rtx_ssrc_it->is_number()) {
                    rtx_ssrc = (uint32_t)rtx_ssrc_it->get<int>();

                }
            }
        }
    }
    if (found_ssrc) {
        pc_->SetVideoSsrc(SDP_ANSWER, ssrc);
        pc_->SetVideoRtxSsrc(SDP_ANSWER, rtx_ssrc);
    } else {
        CSM_THROW_ERROR("fail to get ssrc in video consume");
    }

    int video_mid = rtp_parameter_json["mid"];
    pc_->SetVideoMid(SDP_ANSWER, video_mid);

    auto codecs_array = rtp_parameter_json["codecs"];
    for(auto& codec : codecs_array) {
        std::string mimeType = codec["mimeType"];
        String2Lower(mimeType);

        if (mimeType == "video/h264") {
            int payload_type = codec["payloadType"];
            int clock_rate = codec["clockRate"];

            pc_->SetVideoPayloadType(SDP_ANSWER, payload_type);
            pc_->SetVideoClockRate(SDP_ANSWER, clock_rate);
            auto rtcp_fb_array = codec["rtcpFeedback"];
            for (auto rtcp_fb : rtcp_fb_array) {
                std::string parameter = rtcp_fb["parameter"];
                std::string type      = rtcp_fb["type"];

                if (type == "nack" && parameter.empty()) {
                    pc_->SetVideoNack(SDP_ANSWER, true, payload_type);
                } else if (type == "transport-cc") {
                    pc_->SetCCType(SDP_ANSWER, TCC_TYPE);
                } else if (type == "goog-remb") {
                    pc_->SetCCType(SDP_ANSWER, GCC_TYPE);
                }
            }
        } else if (mimeType == "video/rtx") {
            int payload_type = codec["payloadType"];
            pc_->SetVideoRtxPayloadType(SDP_ANSWER, payload_type);
        } else {
            CSM_THROW_ERROR("unkown mime type:%s", mimeType.c_str());
        }
    }

    auto header_ext_array = ret_json["headerExtensions"];
    for (auto& ext_item : header_ext_array) {
        int id = ext_item["id"];
        std::string uri = ext_item["uri"];
        RTP_EXT_TYPE ext_type = GetRtpExtType(uri);
        RTP_EXT_INFO info;
        info.id = id;
        info.type = ext_type;
        info.uri = uri;
        pc_->AddRtpExtInfo(SDP_ANSWER, id, info);
    }
}

void MsPull::ParseAudioConsume(const std::string& data) {
    auto ret_json = json::parse(data);

    audio_consume_id_ = ret_json["id"];

    std::string prd_id = ret_json["producerId"];
    if (prd_id != audio_produce_id_) {
        CSM_THROW_ERROR("audio consume return produceId(%s) != audio produceId(%s)", prd_id.c_str(), audio_produce_id_.c_str());
    }

    auto rtp_parameter_json = ret_json["rtpParameters"];
    auto encodings_json = ret_json["encodings"];

    uint32_t ssrc = 0;
    bool found_ssrc = false;
    for(auto & encoding_json : encodings_json) {
        auto ssrc_it = encoding_json.find("ssrc");
        if (ssrc_it != encoding_json.end() && ssrc_it->is_number()) {
            ssrc = (uint32_t)ssrc_it->get<int>();
            found_ssrc = true;
        }
    }
    if (found_ssrc) {
        pc_->SetAudioSsrc(SDP_ANSWER, ssrc);
    } else {
        CSM_THROW_ERROR("fail to get ssrc in audio consume");
    }

    int audio_mid = rtp_parameter_json["mid"];
    pc_->SetAudioMid(SDP_ANSWER, audio_mid);

    auto codecs_array = rtp_parameter_json["codecs"];
    for(auto& codec : codecs_array) {
        std::string mimeType = codec["mimeType"];
        String2Lower(mimeType);

        if (mimeType == "audio/opus") {
            int payload_type = codec["payloadType"];
            int clock_rate = codec["clockRate"];

            pc_->SetAudioPayloadType(SDP_ANSWER, payload_type);
            pc_->SetAudioClockRate(SDP_ANSWER, clock_rate);
            auto rtcp_fb_array = codec["rtcpFeedback"];
            for (auto rtcp_fb : rtcp_fb_array) {
                std::string parameter = rtcp_fb["parameter"];
                std::string type      = rtcp_fb["type"];

                if (type == "transport-cc") {
                    pc_->SetCCType(SDP_ANSWER, TCC_TYPE);
                } else if (type == "goog-remb") {
                    pc_->SetCCType(SDP_ANSWER, GCC_TYPE);
                }
            }
        } else {
            CSM_THROW_ERROR("unkown mime type:%s", mimeType.c_str());
        }
    }

    auto header_ext_array = ret_json["headerExtensions"];
    for (auto& ext_item : header_ext_array) {
        int id = ext_item["id"];
        std::string uri = ext_item["uri"];
        RTP_EXT_TYPE ext_type = GetRtpExtType(uri);
        RTP_EXT_INFO info;
        info.id = id;
        info.type = ext_type;
        info.uri = uri;
        pc_->AddRtpExtInfo(SDP_ANSWER, id, info);
    }
}

void MsPull::HandleVideoConsumeResponse(std::shared_ptr<HttpClientResponse> resp_ptr) {
    std::string data_str(resp_ptr->data_.Data(), resp_ptr->data_.DataLen());
    LogInfof(logger_, "http video consume response state:%d, resp:%s", 
            resp_ptr->status_code_, data_str.c_str());

    ParseVideoConsume(data_str);
    pc_->CreateVideoRecvStream();
    AudioConsumeRequest();
}

void MsPull::HandleAudioConsumeResponse(std::shared_ptr<HttpClientResponse> resp_ptr) {
    std::string data_str(resp_ptr->data_.Data(), resp_ptr->data_.DataLen());
    LogInfof(logger_, "http audio consume response state:%d, resp:%s", 
            resp_ptr->status_code_, data_str.c_str());
    ParseAudioConsume(data_str);
    pc_->CreateAudioRecvStream();
}

void MsPull::TransportRequest() {
    std::map<std::string, std::string> headers;
    hc_transport_ = new HttpClient(loop_, host_, port_, this, logger_, true);

    std::stringstream subpath;
    ///rooms/:roomId/broadcasters/:broadcasterId/transports
    subpath << "/rooms/" << roomId_ << "/broadcasters/"
        << userId_ << "/transports"
        << "?broadcasterId=" << userId_;

    headers["content-type"] = "application/json";

    auto req_json = json::object();
    auto rtp_json = json::object();

    req_json["type"]            = "webrtc";

    start_ms_ = now_millisec();
    state_ = BROADCASTER_TRANSPORT;
    hc_transport_->Post(subpath.str(), headers, req_json.dump());
}

void MsPull::TransportConnectRequest() {
    std::map<std::string, std::string> headers;
    hc_trans_connect_ = new HttpClient(loop_, host_, port_, this, logger_, true);

    std::stringstream subpath;
    ///rooms/:roomId/broadcasters/:broadcasterId/transports/:transportId/connect
    subpath << "/rooms/" << roomId_ << "/broadcasters/"
        << userId_ << "/transports/"
        << transport_id_ << "/connect"
        << "?broadcasterId=" << userId_ << "&"
        << "transportId=" << transport_id_;

    headers["content-type"] = "application/json";

    auto req_json        = json::object();
    auto dtlsParams_json = json::object();
    auto fp_array_json   = json::array();
    auto fp_object_json  = json::object();

    fp_object_json["algorithm"] = "sha-256";
    fp_object_json["value"] = pc_->GetFingerSha256();
    fp_array_json.push_back(fp_object_json);

    dtlsParams_json["role"] = "server";
    dtlsParams_json["fingerprints"] = fp_array_json;
    req_json["dtlsParameters"] = dtlsParams_json;

    state_ = BROADCASTER_TRANSPORT_CONNECT;

    LogInfof(logger_, "http post Transport connect subpath:%s, data:%s", subpath.str().c_str(), req_json.dump().c_str());
    hc_trans_connect_->Post(subpath.str(), headers, req_json.dump().c_str());
}

void MsPull::OnState(const std::string& type, const std::string& value) {
    LogDebugf(logger_, "mediasoup state type:%s, value:%s",
            type.c_str(), value.c_str());
    if (type == "dtls" && value == "ready") {
        int64_t diff_t = now_millisec() - start_ms_;
        LogInfof(logger_, "mediasoup connect cost %ldms", diff_t);
        Report("dtls", "ready");
        VideoConsumeRequest();
        return;
    }
    Report(type, value);
}

void MsPull::VideoConsumeRequest() {
    std::map<std::string, std::string> headers;
    hc_video_consume_ = new HttpClient(loop_, host_, port_, this, logger_, true);

    std::stringstream subpath;
    /*/rooms/:roomId/broadcasters/:broadcasterId/transports/:transportId/consume
    */
    subpath << "/rooms/" << roomId_ << "/broadcasters/"
        << userId_ << "/transports/" << transport_id_ << "/consume"
        << "?producerId=" << video_produce_id_;

    headers["content-type"] = "application/json";

    auto req_json        = json::object();

    req_json["broadcasterId"] = userId_;
    req_json["transportId"]   = transport_id_;

    state_ = BROADCASTER_VIDEO_CONSUME;

    LogInfof(logger_, "http post video consume subpath:%s, data:%s", subpath.str().c_str(), req_json.dump().c_str());
    hc_video_consume_->Post(subpath.str(), headers, req_json.dump().c_str());

}

void MsPull::AudioConsumeRequest() {
    std::map<std::string, std::string> headers;
    hc_audio_consume_ = new HttpClient(loop_, host_, port_, this, logger_, true);

    std::stringstream subpath;
    /*/rooms/:roomId/broadcasters/:broadcasterId/transports/:transportId/consume
    */
    subpath << "/rooms/" << roomId_ << "/broadcasters/"
        << userId_ << "/transports/" << transport_id_ << "/consume"
        << "?producerId=" << audio_produce_id_;

    headers["content-type"] = "application/json";

    auto req_json        = json::object();

    req_json["broadcasterId"] = userId_;
    req_json["transportId"]   = transport_id_;

    state_ = BROADCASTER_AUDIO_CONSUME;

    LogInfof(logger_, "http post audio consume subpath:%s, data:%s", subpath.str().c_str(), req_json.dump().c_str());
    hc_audio_consume_->Post(subpath.str(), headers, req_json.dump().c_str());


}

void MsPull::Report(const std::string& type, const std::string& value) {
    if (!report_) {
        return;
    }
    report_->OnReport(name_, type, value);
}
}


