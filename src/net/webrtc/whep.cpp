#include "whep.hpp"
#include "uuid.hpp"
#include "byte_crypto.hpp"

#include <iostream>
#include <stdio.h>

void* make_whep_streamer() {
    cpp_streamer::Whep* whep = new cpp_streamer::Whep();
    return whep;
}

void destroy_whep_streamer(void* streamer) {
    cpp_streamer::Whep* whep = (cpp_streamer::Whep*)streamer;
    delete whep;
}


namespace cpp_streamer
{
#define WHEP_NAME "whep"

Whep::Whep()
{
    ByteCrypto::Init();
    name_ = WHEP_NAME;
    name_ += "_";
    name_ += UUID::MakeUUID();
    LogInfof(logger_, "construct Whep");
}

Whep::~Whep()
{
    LogInfof(logger_, "destruct Whep");
    ReleaseHttpClient();
    if (pc_) {
        delete pc_;
        pc_ = nullptr;
    }
}

void Whep::ReleaseHttpClient() {
    if (hc_) {
        delete hc_;
        hc_ = nullptr;
    }
}

std::string Whep::StreamerName() {
    return name_;
}

void Whep::SetLogger(Logger* logger) {
    logger_ = logger;
}

int Whep::AddSinker(CppStreamerInterface* sinker) {
    if (!sinker) {
        return sinkers_.size();
    }
    sinkers_[sinker->StreamerName()] = sinker;
    return sinkers_.size();
}

int Whep::RemoveSinker(const std::string& name) {
    return sinkers_.erase(name);
}

int Whep::SourceData(Media_Packet_Ptr pkt_ptr) {
    return 0;
}

void Whep::StartNetwork(const std::string& url, void* loop_handle) {
    if (pc_) {
        delete pc_;
        pc_ = nullptr;
    }

    loop_ = (uv_loop_t*)loop_handle;

    pc_ = new PeerConnection((uv_loop_t*)loop_handle, logger_, this);
    pc_->SetMediaCallback(this);

    bool https_enable = false;
    if (!GetHostInfoByUrl(url, host_, port_, subpath_, https_enable)) {
        CSM_THROW_ERROR("fail to get whep url by:%s", url.c_str());
    }

    Start(host_, port_, subpath_, https_enable);

    return;
}

void Whep::AddOption(const std::string& key, const std::string& value) {
    auto iter = options_.find(key);
    if (iter == options_.end()) {
        std::stringstream ss;
        ss << "the option key:" << key << " does not exist";
        throw CppStreamException(ss.str().c_str());
    }
    options_[key] = value;
    LogInfof(logger_, "set whep options key:%s, value:%s", key.c_str(), value.c_str());
}

void Whep::SetReporter(StreamerReport* reporter) {
    report_ = reporter;
}

bool Whep::GetHostInfoByUrl(const std::string& url, std::string& host, 
            uint16_t& port, std::string& subpath, bool& https_enable) {
    const std::string https_scheme("https://");
    const std::string http_scheme("http://");
    std::string whep_url(url);
    std::string scheme;

    size_t pos = whep_url.find(https_scheme);
    if (pos != 0) {
        pos = whep_url.find(http_scheme);
        if (pos != 0) {
            LogErrorf(logger_, "find to find http/https scheme, url:%s", url.c_str());
            return false;
        }
        scheme = http_scheme;
        https_enable = false;
    } else {
        scheme = https_scheme;
        https_enable = true;
    }
    whep_url = whep_url.substr(scheme.length());
    std::vector<std::string> path_vec;

    StringSplit(whep_url, "/", path_vec);
    if (path_vec.size() < 2) {
        LogErrorf(logger_, "fail to get subpath, url:%s", url.c_str());
        return false;
    }
    host = path_vec[0];
    subpath = "";

    for (size_t i = 1; i < path_vec.size(); i++) {
        subpath += "/";
        subpath += path_vec[i];
    }

    pos = host.find(":");
    if (pos == host.npos) {
        port = 443;
    } else {
        std::string port_str = host.substr(pos + 1);
        host = host.substr(0, pos);
        port = atoi(port_str.c_str());
    }
    return true;
}

int Whep::Start(const std::string& host, uint16_t port, const std::string& subpath, bool https_enable) {
    std::string offer_sdp = pc_->CreateOfferSdp(RECV_ONLY);

    if (offer_sdp.empty()) {
        LogErrorf(logger_, "CreateOfferSdp recvonly error");
        return -1;
    }
    LogInfof(logger_, "CreateOfferSdp:%s", offer_sdp.c_str());

    ReleaseHttpClient();
    std::map<std::string, std::string> headers;
    hc_ = new HttpClient(loop_, host_, port_, this, logger_, https_enable);
    LogInfof(logger_, "whep http post host:%s, port:%d, subpath:%s",
            host_.c_str(), port_, subpath.c_str());
    start_ms_ = now_millisec();
    return hc_->Post(subpath, headers, offer_sdp);
}

void Whep::OnHttpRead(int ret, std::shared_ptr<HttpClientResponse> resp_ptr) {
    if (ret < 0) {
        LogInfof(logger_, "OnHttpRead return error:%d", ret);
        ReleaseHttpClient();
        return;
    }
    std::string resp_data(resp_ptr->data_.Data(), resp_ptr->data_.DataLen());

    LogInfof(logger_, "http status:%d, status desc:%s, content len:%d, subpath:%s",
            resp_ptr->status_code_, resp_ptr->status_.c_str(), resp_ptr->content_length_,
            subpath_.c_str());
    LogInfof(logger_, "http response subpath:%s, data:%s", subpath_.c_str(), resp_data.c_str());

    pc_->ParseAnswerSdp(resp_data);
}

void Whep::OnState(const std::string& type, const std::string& value) {
    LogDebugf(logger_, "whep state subpath:%s, type:%s, value:%s",
            subpath_.c_str(), type.c_str(), value.c_str());

    if (type == "dtls" && value == "ready") {
        int64_t diff_t = now_millisec() - start_ms_;
        LogInfof(logger_, "whep subpath:%s, connect cost %ldms", 
                subpath_.c_str(), diff_t);
    }
    //Report("dtls", "ready");
    if (report_) {
        report_->OnReport(name_, type, value);
    }
}

void Whep::OnReceiveMediaPacket(Media_Packet_Ptr pkt_ptr) {
    if (sinkers_.empty()) {
        return;
    }
    for (auto& sinker : sinkers_) {
        sinker.second->SourceData(pkt_ptr);
    }
}

}
