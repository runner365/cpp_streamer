#include "ms_push.hpp"
#include "uuid.hpp"
#include <uv.h>
#include <sstream>

#include <iostream>
#include <stdio.h>

void* make_mspush_streamer() {
    cpp_streamer::MsPush* ms = new cpp_streamer::MsPush();
    return ms;
}

void destroy_mspush_streamer(void* streamer) {
    cpp_streamer::MsPush* ms = (cpp_streamer::MsPush*)streamer;
    delete ms;
}


namespace cpp_streamer
{
#define MEDIASOUP_PUSH_NAME "mspush"

void SourceBroadcasterData(uv_async_t *handle) {
    MsPush* ms = (MsPush*)(handle->data);
    ms->HandleMediaData();
}

MsPush::MsPush()
{
    ByteCrypto::Init();
    name_ = MEDIASOUP_PUSH_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
}

MsPush::~MsPush()
{
    LogInfof(logger_, "destruct mediasoup push");
    ReleaseHttpClient(hc_req_);
    ReleaseHttpClient(hc_transport_);
    ReleaseHttpClient(hc_video_prd_);
    ReleaseHttpClient(hc_audio_prd_);
    if (pc_) {
        delete pc_;
        pc_ = nullptr;
    }
}

Media_Packet_Ptr MsPush::GetMediaPacket() {
    std::lock_guard<std::mutex> lock(mutex_);

    Media_Packet_Ptr pkt_ptr;
    if (packet_queue_.empty()) {
        return pkt_ptr;
    }
        
    pkt_ptr = packet_queue_.front();
    packet_queue_.pop();

    return pkt_ptr;
}

void MsPush::HandleMediaData() {
    while(true) {
        Media_Packet_Ptr pkt_ptr = GetMediaPacket();
        if (!pkt_ptr) {
            break;
        }

        if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
            pc_->SendVideoPacket(pkt_ptr);
        } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
            pc_->SendAudioPacket(pkt_ptr);
        } else {
            LogErrorf(logger_, "input media type error:%s",
                    avtype_tostring(pkt_ptr->av_type_).c_str());
        }
    }
    return;
}

void MsPush::ReleaseHttpClient(HttpClient*& hc) {
    if (hc) {
        delete hc;
        hc = nullptr;
    }
}

std::string MsPush::StreamerName() {
    return name_;
}

void MsPush::SetLogger(Logger* logger) {
    logger_ = logger;
}

int MsPush::AddSinker(CppStreamerInterface* sinker) {
    if (!sinker) {
        return sinkers_.size();
    }
    sinkers_[sinker->StreamerName()] = sinker;
    return sinkers_.size();
}

int MsPush::RemoveSinker(const std::string& name) {
    return sinkers_.erase(name);
}

int MsPush::SourceData(Media_Packet_Ptr pkt_ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    Media_Packet_Ptr new_ptr = pkt_ptr->copy();

    packet_queue_.push(new_ptr);

    async_.data = (void*)this;
    uv_async_send(&async_);
    return (int)packet_queue_.size();
}

void MsPush::StartNetwork(const std::string& url, void* loop_handle) {
    if (pc_) {
        delete pc_;
        pc_ = nullptr;
    }

    GetHostInfoByUrl(url, host_, port_, roomId_, userId_);
    LogInfof(logger_, "http post host:%s, port:%d, roomId:%s, userId:%s",
            host_.c_str(), port_, roomId_.c_str(), userId_.c_str());
    loop_ = (uv_loop_t*)loop_handle;
    uv_async_init(loop_, &async_, SourceBroadcasterData);

    pc_ = new PeerConnection((uv_loop_t*)loop_handle, logger_, this);

    BroadCasterRequest();
    return;
}

void MsPush::BroadCasterRequest() {
    std::map<std::string, std::string> headers;
    hc_req_ = new HttpClient(loop_, host_, port_, this, logger_, true);

    std::stringstream subpath;
    ///rooms/:roomId/broadcasters
    subpath << "/rooms/" << roomId_ << "/broadcasters";

    headers["content-type"] = "application/json";
    //'content-type: application/json'
    auto req_json    = json::object();
    auto device_json = json::object();
    req_json["id"]          = userId_;
    req_json["displayName"] = userId_;
    device_json["name"]     = "cpp_streamer";
    req_json["device"]      = device_json;

    state_ = BROADCASTER_REQUEST;

    LogInfof(logger_, "http post subpath:%s, json data:%s",
            subpath.str().c_str(), req_json.dump().c_str());
    hc_req_->Post(subpath.str(), headers, req_json.dump());
}

void MsPush::TransportRequest() {
    std::map<std::string, std::string> headers;
    hc_transport_ = new HttpClient(loop_, host_, port_, this, logger_, true);

    std::stringstream subpath;
    ///rooms/:roomId/broadcasters/:broadcasterId/transports
    subpath << "/rooms/" << roomId_ << "/broadcasters/"
        << userId_ << "/transports"
        << "?broadcasterId=" << userId_;

    headers["content-type"] = "application/json";

    auto req_json    = json::object();
    req_json["type"]        = "webrtc";

    state_ = BROADCASTER_TRANSPORT;
    hc_transport_->Post(subpath.str(), headers, req_json.dump());
}

void MsPush::TransportConnectRequest() {
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

void MsPush::VideoProduceRequest() {
    std::map<std::string, std::string> headers;
    hc_video_prd_ = new HttpClient(loop_, host_, port_, this, logger_, true);

    std::stringstream subpath;
    ///rooms/:roomId/broadcasters/:broadcasterId/transports/:transportId/producers
    subpath << "/rooms/" << roomId_ << "/broadcasters/"
        << userId_ << "/transports/" << transport_id_ << "/producers"
        << "?broadcasterId=" << userId_ << "&" << "transportId=" << transport_id_;

    headers["content-type"] = "application/json";

    auto req_json        = json::object();
    auto rtp_params_json = json::object();

    req_json["kind"] = "video";
    GetVideoRtpParameters(rtp_params_json);
    req_json["rtpParameters"] = rtp_params_json;

    state_ = BROADCASTER_VIDEO_PRODUCE;

    LogInfof(logger_, "http post video produce subpath:%s, data:%s", subpath.str().c_str(), req_json.dump().c_str());
    hc_video_prd_->Post(subpath.str(), headers, req_json.dump().c_str());
}

void MsPush::AudioProduceRequest() {
    std::map<std::string, std::string> headers;
    hc_audio_prd_ = new HttpClient(loop_, host_, port_, this, logger_, true);

    std::stringstream subpath;
    ///rooms/:roomId/broadcasters/:broadcasterId/transports/:transportId/producers
    subpath << "/rooms/" << roomId_ << "/broadcasters/"
        << userId_ << "/transports/" << transport_id_ << "/producers"
        << "?broadcasterId=" << userId_ << "&" << "transportId=" << transport_id_;

    headers["content-type"] = "application/json";

    auto req_json        = json::object();
    auto rtp_params_json = json::object();

    req_json["kind"] = "audio";
    GetAudioRtpParameters(rtp_params_json);
    req_json["rtpParameters"] = rtp_params_json;

    state_ = BROADCASTER_AUDIO_PRODUCE;

    LogInfof(logger_, "http post audio produce subpath:%s, data:%s", subpath.str().c_str(), req_json.dump().c_str());
    hc_audio_prd_->Post(subpath.str(), headers, req_json.dump().c_str());
}

void MsPush::AddOption(const std::string& key, const std::string& value) {
    auto iter = options_.find(key);
    if (iter == options_.end()) {
        std::stringstream ss;
        ss << "the option key:" << key << " does not exist";
        throw CppStreamException(ss.str().c_str());
    }
    options_[key] = value;
    LogInfof(logger_, "set mediaspu broadcaster options key:%s, value:%s", key.c_str(), value.c_str());
}

void MsPush::SetReporter(StreamerReport* reporter) {
    report_ = reporter;
}

bool MsPush::GetHostInfoByUrl(const std::string& url, std::string& host, uint16_t& port, std::string& roomId, std::string& userId) {
    const std::string https_scheme("https://");
    std::string hostUrl(url);

    //https://xxxx.com:8080/?roomId=1000&userId=10000
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
        }
    }
    return true;
}

void MsPush::OnHttpRead(int ret, std::shared_ptr<HttpClientResponse> resp_ptr) {
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
    } else if (state_ == BROADCASTER_VIDEO_PRODUCE) {
        HandleVideoProduceResponse(resp_ptr);
    } else if (state_ == BROADCASTER_AUDIO_PRODUCE) {
        HandleAudioProduceResponse(resp_ptr);
    }

}

void MsPush::HandleAudioProduceResponse(std::shared_ptr<HttpClientResponse> resp_ptr) {
    std::string data_str(resp_ptr->data_.Data(), resp_ptr->data_.DataLen());
    LogInfof(logger_, "http audio produce response state:%d, resp:%s", 
            resp_ptr->status_code_, data_str.c_str());
    if (resp_ptr->status_code_ != 200) {
        LogErrorf(logger_, "http audio produce response state:%d error", resp_ptr->status_code_);
        return;
    }
    pc_->CreateSendStream2();
    Report("audio_produce", "ready");
}

void MsPush::HandleVideoProduceResponse(std::shared_ptr<HttpClientResponse> resp_ptr) {
    std::string data_str(resp_ptr->data_.Data(), resp_ptr->data_.DataLen());
    LogInfof(logger_, "http video produce response state:%d, resp:%s", 
            resp_ptr->status_code_, data_str.c_str());
    if (resp_ptr->status_code_ != 200) {
        LogErrorf(logger_, "http video produce response state:%d error", resp_ptr->status_code_);
        return;
    }
    Report("video_produce", "ready");
    AudioProduceRequest();
}

void MsPush::HandleTransportConnectResponse(std::shared_ptr<HttpClientResponse> resp_ptr) {
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

void MsPush::HandleTransportResponse(std::shared_ptr<HttpClientResponse> resp_ptr) {
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

void MsPush::HandleBroadCasterResponse(std::shared_ptr<HttpClientResponse> resp_ptr) {
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

void MsPush::OnState(const std::string& type, const std::string& value) {
    LogDebugf(logger_, "mediasoup state type:%s, value:%s",
            type.c_str(), value.c_str());
    if (type == "dtls" && value == "ready") {
        int64_t diff_t = now_millisec() - start_ms_;
        LogInfof(logger_, "mediasoup connect cost %ldms", diff_t);
        Report("dtls", "ready");
        VideoProduceRequest();
        return;
    }
    Report(type, value);
}

void MsPush::Report(const std::string& type, const std::string& value) {
    if (report_) {
        report_->OnReport(name_, type, value);
    }
}

bool MsPush::GetVideoRtpParameters(json& rtp_parameters_json) {
    rtp_parameters_json["mid"] = std::to_string(pc_->GetVideoMid(SDP_OFFER));

    /*start: codecs*/
    auto codecs_json = json::array();
    auto h264_codec_json = json::object();
    auto codec_params_json = json::object();
    auto rtx_codec_json = json::object();

    std::stringstream h264_mimetype;
    h264_mimetype << "video/" << pc_->GetVideoCodecType(SDP_OFFER);

    h264_codec_json["mimeType"]    = h264_mimetype.str();
    h264_codec_json["payloadType"] = pc_->GetVideoPayloadType(SDP_OFFER);
    h264_codec_json["clockRate"]   = pc_->GetVideoClockRate(SDP_OFFER);

    codec_params_json["packetization-mode"] = 1;
    codec_params_json["profile-level-id"]   = "42e01f";
    codec_params_json["level-asymmetry-allowed"]   = 1;
    h264_codec_json["parameters"]  = codec_params_json;

    auto rtcpfb_array_json = json::array();
    std::vector<RtcpFbInfo> rtcpfb_vec = pc_->GetVideoRtcpFbInfo();
    for (auto& fb_info : rtcpfb_vec) {
        auto rtcpfb_json = json::object();
        rtcpfb_json["type"] = fb_info.attr_string;
        if (fb_info.attr_string == "ccm") {
            rtcpfb_json["parameter"] = "fir";
        } else if (fb_info.attr_string == "nack") {
            if (fb_info.payload_type == pc_->GetVideoRtxPayloadType(SDP_OFFER)) {
                rtcpfb_json["parameter"] = "fir";
            } else {
                rtcpfb_json["parameter"] = "";
            }
        } else {
            rtcpfb_json["parameter"] = "";
        }
        rtcpfb_array_json.push_back(rtcpfb_json);
    }
    h264_codec_json["rtcpFeedback"] = rtcpfb_array_json;
    codecs_json.push_back(h264_codec_json);

    auto rtx_codec_param_json = json::object();
    rtx_codec_json["mimeType"] = "video/rtx";
    rtx_codec_json["payloadType"] = pc_->GetVideoRtxPayloadType(SDP_OFFER);
    rtx_codec_json["clockRate"]   = pc_->GetVideoClockRate(SDP_OFFER);
    rtx_codec_param_json["apt"]   = pc_->GetVideoPayloadType(SDP_OFFER);

    rtx_codec_json["parameters"]  = rtx_codec_param_json;
    rtx_codec_json["rtcpFeedback"] = json::array();

    codecs_json.push_back(rtx_codec_json);
    rtp_parameters_json["codecs"] = codecs_json;
    /*end: codecs*/

    /*start: header externtion*/
    int offset_id;
    int abs_send_time_id;
    int video_rotation_id;
    int tcc_id;
    int playout_delay_id;
    int video_content_id;
    int video_timing_id;
    int color_space_id;
    int sdes_id;
    int rtp_streamid_id;
    int rp_rtp_streamid_id;
    int audio_level;

    auto header_exts_array_json = json::array();
    pc_->GetHeaderExternId(offset_id, abs_send_time_id,
            video_rotation_id, tcc_id,
            playout_delay_id, video_content_id,
            video_timing_id, color_space_id,
            sdes_id, rtp_streamid_id,
            rp_rtp_streamid_id, audio_level);
    do {
        auto mid_header_ext_json          = json::object();
        mid_header_ext_json["uri"]        = "urn:ietf:params:rtp-hdrext:sdes:mid";
        mid_header_ext_json["id"]         = sdes_id;
        mid_header_ext_json["encrypt"]    = false;
        mid_header_ext_json["parameters"] = json::object();
        header_exts_array_json.push_back(mid_header_ext_json);

        auto rtp_streamid_json          = json::object();
        rtp_streamid_json["uri"]        = "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id";
        rtp_streamid_json["id"]         = rtp_streamid_id;
        rtp_streamid_json["encrypt"]    = false;
        rtp_streamid_json["parameters"] = json::object();
        header_exts_array_json.push_back(rtp_streamid_json);

        auto rp_rtp_streamid_json       = json::object();
        rp_rtp_streamid_json["uri"]        = "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id";
        rp_rtp_streamid_json["id"]         = rp_rtp_streamid_id;
        rp_rtp_streamid_json["encrypt"]    = false;
        rp_rtp_streamid_json["parameters"] = json::object();
        header_exts_array_json.push_back(rp_rtp_streamid_json);

        auto abs_send_time_json            = json::object();
        abs_send_time_json["uri"]        = "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
        abs_send_time_json["id"]         = abs_send_time_id;
        abs_send_time_json["encrypt"]    = false;
        abs_send_time_json["parameters"] = json::object();
        header_exts_array_json.push_back(abs_send_time_json);

        auto tcc_json          = json::object();
        tcc_json["uri"]        = "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";
        tcc_json["id"]         = tcc_id;
        tcc_json["encrypt"]    = false;
        tcc_json["parameters"] = json::object();
        header_exts_array_json.push_back(tcc_json);

        auto video_orientation_json          = json::object();
        video_orientation_json["uri"]        = "urn:3gpp:video-orientation";
        video_orientation_json["id"]         = video_rotation_id;
        video_orientation_json["encrypt"]    = false;
        video_orientation_json["parameters"] = json::object();
        header_exts_array_json.push_back(video_orientation_json);

        auto toffset_json          = json::object();
        toffset_json["uri"]        = "urn:ietf:params:rtp-hdrext:toffset";
        toffset_json["id"]         = offset_id;
        toffset_json["encrypt"]    = false;
        toffset_json["parameters"] = json::object();
        header_exts_array_json.push_back(toffset_json);
    } while(0);
    rtp_parameters_json["headerExtensions"] = header_exts_array_json;
    /*end: header externtion*/

    /*start: encodings*/
    auto encodings_array_json = json::array();
    auto video_encoding_json  = json::object();
    auto rtx_json = json::object();
    rtx_json["ssrc"] = pc_->GetVideoRtxSsrc(SDP_OFFER);
    video_encoding_json["ssrc"] = pc_->GetVideoSsrc(SDP_OFFER);
    video_encoding_json["dtx"]  = false;
    video_encoding_json["rtx"]  = rtx_json;
    encodings_array_json.push_back(video_encoding_json);

    rtp_parameters_json["encodings"] = encodings_array_json;
    /*end: encodings*/

    /*start: rtcp*/
    auto rtcp_json           = json::object();
    rtcp_json["cname"]       = pc_->GetVideoCName();
    rtcp_json["reducedSize"] = true;
    rtp_parameters_json["rtcp"] = rtcp_json;
    /*end: rtcp*/

    return true;
}

bool MsPush::GetAudioRtpParameters(json& rtp_parameters_json) {
    rtp_parameters_json["mid"] = std::to_string(pc_->GetAudioMid(SDP_OFFER));

    /*start: codecs*/
    auto codecs_json = json::array();
    auto opus_codec_json = json::object();

    std::stringstream opus_mimetype;
    opus_mimetype << "audio/" << pc_->GetAudioCodecType(SDP_OFFER);

    opus_codec_json["mimeType"]    = opus_mimetype.str();
    opus_codec_json["payloadType"] = pc_->GetAudioPayloadType(SDP_OFFER);
    opus_codec_json["clockRate"]   = pc_->GetAudioClockRate(SDP_OFFER);
    opus_codec_json["channels"] = 2;

    auto codec_params_json = json::object();
    codec_params_json["minptime"] = 10;
    codec_params_json["useinbandfec"] = 1;
    codec_params_json["sprop-stereo"] = 1;
    codec_params_json["usedtx"] = 1;
    opus_codec_json["parameters"]  = codec_params_json;

    auto rtcpfb_array_json = json::array();
    std::vector<RtcpFbInfo> rtcpfb_vec = pc_->GetAudioRtcpFbInfo();
    for (auto& fb_info : rtcpfb_vec) {
        auto rtcpfb_json = json::object();
        rtcpfb_json["type"] = fb_info.attr_string;
        rtcpfb_json["parameter"] = "";
        rtcpfb_array_json.push_back(rtcpfb_json);
    }
    opus_codec_json["rtcpFeedback"] = rtcpfb_array_json;
    codecs_json.push_back(opus_codec_json);

    rtp_parameters_json["codecs"] = codecs_json;
    /*end: codecs*/

    /*start: header externtion*/
    int offset_id;
    int abs_send_time_id;
    int video_rotation_id;
    int tcc_id;
    int playout_delay_id;
    int video_content_id;
    int video_timing_id;
    int color_space_id;
    int sdes_id;
    int rtp_streamid_id;
    int rp_rtp_streamid_id;
    int audio_level;

    auto header_exts_array_json = json::array();
    pc_->GetHeaderExternId(offset_id, abs_send_time_id,
            video_rotation_id, tcc_id,
            playout_delay_id, video_content_id,
            video_timing_id, color_space_id,
            sdes_id, rtp_streamid_id,
            rp_rtp_streamid_id, audio_level);
    do {
        auto mid_header_ext_json          = json::object();
        mid_header_ext_json["uri"]        = "urn:ietf:params:rtp-hdrext:sdes:mid";
        mid_header_ext_json["id"]         = sdes_id;
        mid_header_ext_json["encrypt"]    = false;
        mid_header_ext_json["parameters"] = json::object();
        header_exts_array_json.push_back(mid_header_ext_json);

        (void)rtp_streamid_id;
        (void)rp_rtp_streamid_id;

        auto abs_send_time_json            = json::object();
        abs_send_time_json["uri"]        = "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
        abs_send_time_json["id"]         = abs_send_time_id;
        abs_send_time_json["encrypt"]    = false;
        abs_send_time_json["parameters"] = json::object();
        header_exts_array_json.push_back(abs_send_time_json);

        auto tcc_json          = json::object();
        tcc_json["uri"]        = "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";
        tcc_json["id"]         = tcc_id;
        tcc_json["encrypt"]    = false;
        tcc_json["parameters"] = json::object();
        header_exts_array_json.push_back(tcc_json);

        auto audio_level_json          = json::object();
        audio_level_json["uri"]        = "urn:ietf:params:rtp-hdrext:ssrc-audio-level";
        audio_level_json["id"]         = audio_level;
        audio_level_json["encrypt"]    = false;
        audio_level_json["parameters"] = json::object();
        header_exts_array_json.push_back(audio_level_json);

        (void)video_rotation_id;
        (void)offset_id;
    } while(0);
    rtp_parameters_json["headerExtensions"] = header_exts_array_json;
    /*end: header externtion*/

    /*start: encodings*/
    auto encodings_array_json = json::array();
    auto audio_encoding_json  = json::object();
    audio_encoding_json["ssrc"] = pc_->GetAudioSsrc(SDP_OFFER);
    audio_encoding_json["dtx"]  = false;
    encodings_array_json.push_back(audio_encoding_json);

    rtp_parameters_json["encodings"] = encodings_array_json;
    /*end: encodings*/

    /*start: rtcp*/
    auto rtcp_json           = json::object();
    rtcp_json["cname"]       = pc_->GetAudioCName();
    rtcp_json["reducedSize"] = true;
    rtp_parameters_json["rtcp"] = rtcp_json;
    /*end: rtcp*/

    return true;
}

}
