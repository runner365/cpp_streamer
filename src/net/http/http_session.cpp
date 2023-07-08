#include "HttpSession.hpp"
#include "http_server.hpp"
#include "http_common.hpp"
#include "stringex.hpp"
#include <map>

namespace cpp_streamer
{
int GetUriAndParams(const std::string& input, std::string& uri, std::map<std::string, std::string>& params) {
    size_t pos = input.find("?");
    if (pos == input.npos) {
        uri = input;
        return 0;
    }
    uri = input.substr(0, pos);

    std::string param_str = input.substr(pos+1);
    std::vector<std::string> item_vec;

    StringSplit(param_str, "&", item_vec);

    for (auto& item : item_vec) {
        std::vector<std::string> param_vec;
        StringSplit(item, "=", param_vec);
        if (param_vec.size() != 2) {
            return -1;
        }
        params[param_vec[0]] =param_vec[1];
    }

    return 0;
}

HttpSession::HttpSession(uv_loop_t* loop, uv_stream_t* handle, HttpCallbackI* callback, Logger* logger) : callback_(callback)
                                                                                                          , logger_(logger)
{
    request_ = new HttpRequest(this);
    session_ptr_ = std::make_shared<TcpSession>(loop, handle, this);
    remote_address_ = session_ptr_->GetRemoteEndpoint();

    TryRead();
}

HttpSession::HttpSession(uv_loop_t* loop, uv_stream_t* handle, HttpCallbackI* callback,
                const std::string& key_file, const std::string& cert_file, Logger* logger) : callback_(callback)
                                                                                             , logger_(logger)
{
    request_ = new HttpRequest(this);
    session_ptr_ = std::make_shared<TcpSession>(loop, handle, this, key_file, cert_file);
    remote_address_ = session_ptr_->GetRemoteEndpoint();

    TryRead();
}

HttpSession::~HttpSession() {
    Close();
}

void HttpSession::TryRead() {
    session_ptr_->AsyncRead();
}

void HttpSession::Write(const char* data, size_t len) {
    session_ptr_->AsyncWrite(data, len);
}

void HttpSession::Close() {
    if (is_closed_) {
        return;
    }
    is_closed_ = true;
    
    if (response_ptr_.get()) {
        response_ptr_->SetClose(true);
    }
    if (request_) {
        delete request_;
        request_ = nullptr;
    }
    session_ptr_->Close();
    callback_->OnClose(remote_address_);
    return;
}

void HttpSession::OnWrite(int ret_code, size_t sent_size) {
    if (ret_code != 0) {
        LogWarnf(logger_, "http session write callback return code:%d", ret_code);
        Close();
    }
    KeepAlive();
    if (!response_ptr_) {
        return;
    }

    response_ptr_->remain_bytes_ -= sent_size;
    continue_flag_ = response_ptr_->continue_flag_;
    if (!continue_flag_) {
        if (!response_ptr_) {
            return;
        }    
        if (response_ptr_->remain_bytes_ <= 0) {
            Close();
        }
    }
    
    return;
}

int HttpSession::HandleRequest(const char* data, size_t data_size, bool& continue_flag) {
    KeepAlive();

    continue_flag = true;
    if (!header_is_ready_) {
        header_data_.AppendData(data, data_size);
        int ret = AnalyzeHeader();
        if (ret != 0) {
            return -1;
        }

        if (!header_is_ready_) {
            return 0;
        }

        char* start = header_data_.Data() + content_start_;
        int len     = header_data_.DataLen() - content_start_;
        content_data_.AppendData(start, len);
    } else {
        content_data_.AppendData(data, data_size);
    }

    if (request_->method_ == "OPTIONS") {
        if (!response_ptr_) {
            response_ptr_ = std::make_shared<HttpResponse>(this);
        }
        /*
        HTTP/1.1 200
        Date: Fri, 22 Apr 2022 11:29:50 GMT
        Content-Length: 0
        Connection: keep-alive
        Vary: Origin
        Vary: Access-Control-Request-Method
        Vary: Access-Control-Request-Headers
        Access-Control-Allow-Origin: *
        Access-Control-Allow-Methods: POST
        Access-Control-Allow-Headers: content-type
        Access-Control-Max-Age: 1800
        Allow: GET, HEAD, POST, PUT, DELETE, TRACE, OPTIONS, PATCH
        */
        response_ptr_->AddHeader("Access-Control-Allow-Origin", "*");
        response_ptr_->AddHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
        response_ptr_->AddHeader("Access-Control-Allow-Headers", "*");
        response_ptr_->AddHeader("Access-Control-Allow-Private-Network", "true");
        response_ptr_->AddHeader("Allow", "GET, HEAD, POST, PUT, DELETE, TRACE, OPTIONS, PATCH");
        response_ptr_->Write(nullptr, 0);
        header_is_ready_ = false;
        continue_flag = true;
        header_data_.Reset();
        return 0;
    }

    if (request_->method_ == "GET") {
        HTTP_HANDLE_PTR handle_ptr = callback_->GetHandle(request_);
        if (!handle_ptr) {
            return -1;
        }
        //call handle
        response_ptr_ = std::make_shared<HttpResponse>(this);
        try {
            handle_ptr(request_, response_ptr_);
        } catch(const std::exception& e) {
            std::cout << "http get exception:" << e.what() << "\r\n";
        }
        continue_flag = response_ptr_->continue_flag_;
        return 0;
    }

    if ((int)content_data_.DataLen() >= request_->content_length_) {
        HTTP_HANDLE_PTR handle_ptr = callback_->GetHandle(request_);
        if (!handle_ptr) {
            return -1;
        }
        //call handle
        if (!response_ptr_) {
            response_ptr_ = std::make_shared<HttpResponse>(this);
        }
        request_->content_body_ = content_data_.Data();
        try {
            handle_ptr(request_, response_ptr_);
        } catch(const std::exception& e) {
            std::cout << "http post exception:" << e.what() << "\r\n";
        }
        continue_flag = response_ptr_->continue_flag_;

        if (!continue_flag) {
            UpdateMax(0);
        }
        return 0;
    }

    return 0;
}

void HttpSession::OnRead(int ret_code, const char* data, size_t data_size) {
    if (ret_code != 0) {
        Close();
        return;
    }

    int ret = HandleRequest(data, data_size, continue_flag_);
    if (ret < 0) {
        Close();
        return;
    }
    if (continue_flag_) {
        TryRead();
    }
    return;
}

int HttpSession::AnalyzeHeader() {
    std::string info(header_data_.Data(), header_data_.DataLen());
    size_t pos = info.find("\r\n\r\n");
    if (pos == info.npos) {
        header_is_ready_ = false;
        return 0;
    }

    content_start_ = (int)pos + 4;
    header_is_ready_ = true;
    std::string header_str(header_data_.Data(), pos);
    std::vector<std::string> header_vec;

    StringSplit(header_str, "\r\n", header_vec);
    if (header_vec.empty()) {
        return -1;
    }

    const std::string& first_line = header_vec[0];
    std::vector<std::string> version_vec;
    StringSplit(first_line, " ", version_vec);
    if (version_vec.size() != 3) {
        LogErrorf(logger_, "version line error:%s", first_line.c_str());
        return -1;
    }

    request_->method_ = version_vec[0];

    if (GetUriAndParams(version_vec[1], request_->uri_, request_->params) < 0) {
        LogErrorf(logger_, "the uri is error:%s", version_vec[1].c_str());
        return -1;
    }

    const std::string& version_info = version_vec[2];
    pos = version_info.find("HTTP");
    if (pos == version_info.npos) {
        LogErrorf(logger_, "http version error:%s", version_info.c_str());
        return -1;
    }

    request_->version_ = version_info.substr(pos+5);
    for (size_t index = 1; index < header_vec.size(); index++) {
        const std::string& info_line = header_vec[index];
        pos = info_line.find(":");
        if (pos == info_line.npos) {
            LogErrorf(logger_, "header line error:%s", info_line.c_str());
            return -1;
        }
        std::string key = info_line.substr(0, pos);
        std::string value = info_line.substr(pos+2);
        if (key == "Content-Length") {
            request_->content_length_ = atoi(value.c_str());
        }
        request_->headers_.insert(std::make_pair(key, value));
    }

    return 0;
}

}
