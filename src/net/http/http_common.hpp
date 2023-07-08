#ifndef HTTP_COMMOM_HPP
#define HTTP_COMMOM_HPP
#include "tcp_session.hpp"
#include "http_session.hpp"
#include <string>
#include <map>
#include <stdint.h>
#include <sstream>
#include <map>

namespace cpp_streamer
{
class HttpRequest
{
friend class HttpSession;
public:
    HttpRequest(HttpSession* session) {
        session_ = session;
    }
    ~HttpRequest() {
    }

public:
    void TryRead(TcpSessionCallbackI* callback) {
        callback_ = callback;
        session_->TryRead();
        return;
    }

    std::string remote_address() const {
        return session_->RemoteEndpoint();
    }

public:
    std::string method_;
    std::string uri_;
    std::map<std::string, std::string> params;
    std::string version_;
    char* content_body_ = nullptr;
    int content_length_ = 0;
    std::map<std::string, std::string> headers_;

private:
    HttpSession* session_;
    TcpSessionCallbackI* callback_;
};

class HttpResponse
{
public:
    HttpResponse(HttpSession* session) {
        session_ = session;
    }
    ~HttpResponse() {
    }

public:
    void SetStatusCode(int status_code) { status_code_ = status_code; }
    void SetStatus(const std::string& status) { status_ = status; }
    void AddHeader(const std::string& key, const std::string& value) { headers_[key] = value; }
    std::map<std::string, std::string> Headers() { return headers_; }

    int Write(const char* data, size_t len, bool continue_flag = false) {
        std::stringstream ss;

        continue_flag_ = continue_flag;
        if (is_close_ || session_ == nullptr) {
            return -1;
        }

        if (!written_header_) {
            ss << proto_ << "/" << version_ << " " << status_code_ << " " << status_ << "\r\n";
            if (!continue_flag) {
                ss << "Content-Length:" << len  << "\r\n";
            }
            
            for (const auto& header : headers_) {
                ss << header.first << ": " << header.second << "\r\n";
            }
            ss << "\r\n";
            size_t len = ss.str().length();
            remain_bytes_ += len;
            session_->Write(ss.str().c_str(), len);
            written_header_ = true;
        }

        if (data && len > 0) {
            remain_bytes_ += len;
            session_->Write(data, len);
        }
        
        return 0;
    }

    void Close() {
        if (is_close_) {
            return;
        }
        is_close_ = true;
        session_->Close();
        session_ = nullptr;
    }

    void SetClose(bool flag) {
        is_close_ = flag;
        session_ = nullptr;
    }

private:
    bool is_close_ = false;
    bool written_header_ = false;
    std::string status_  = "OK";  // e.g. "OK"
    int status_code_     = 200;     // e.g. 200
    std::string proto_   = "HTTP";   // e.g. "HTTP"
    std::string version_ = "1.1"; // e.g. "1.1"
    std::map<std::string, std::string> headers_; //headers

public:
    HttpSession* session_;
    bool continue_flag_ = false;
    int64_t remain_bytes_ = 0;
};

using HTTP_HANDLE_PTR = void (*)(const HttpRequest* request, std::shared_ptr<HttpResponse> response_ptr);

class HttpCallbackI
{
public:
    virtual void OnClose(const std::string& endpoint) = 0;
    virtual HTTP_HANDLE_PTR GetHandle(HttpRequest* request) = 0;
};

inline std::string GetUri(std::string& uri) {
    if (uri == "/") {
        return uri;
    }
    size_t pos = uri.find("/");
    if (pos == 0) {
        uri = uri.substr(1);
    }
    pos = uri.rfind("/");
    if (pos == (uri.length() - 1)) {
        uri = uri.substr(0, pos);
    }

    return uri;
}

}
#endif
