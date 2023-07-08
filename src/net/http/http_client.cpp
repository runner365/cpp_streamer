#include "http_client.hpp"
#include "utils/logger.hpp"
#include "utils/stringex.hpp"

#include <string>
#include <sstream>
#include <uv.h>
#include <assert.h>

namespace cpp_streamer
{

HttpClient::HttpClient(uv_loop_t* loop,
                       const std::string& host,
                       uint16_t port,
                       HttpClientCallbackI* cb,
                       Logger* logger,
                       bool ssl_enable): host_(host)
                                         , port_(port)
                                         , cb_(cb)
                                         , logger_(logger)
{
    client_ = new TcpClient(loop, this, logger_, ssl_enable);
}

HttpClient::~HttpClient()
{
    LogInfof(logger_, "HttpClient destruct...");
    if (client_) {
        delete client_;
        client_ = nullptr;
    }
}

int HttpClient::Get(const std::string& subpath, std::map<std::string, std::string> headers) {
    method_  = HTTP_GET;
    subpath_ = subpath;

    LogInfof(logger_, "http get connect host:%s, port:%d, subpath:%s", host_.c_str(), port_, subpath.c_str());
    client_->Connect(host_, port_);
    return 0;
}

int HttpClient::Post(const std::string& subpath, std::map<std::string, std::string> headers, const std::string& data) {
    method_    = HTTP_POST;
    subpath_   = subpath;
    post_data_ = data;
    headers_   = headers;

    client_->Connect(host_, port_);
    LogInfof(logger_, "http post connect host:%s, port:%d, subpath:%s, post data:%s", 
            host_.c_str(), port_, subpath.c_str(), data.c_str());
    return 0;
}

void HttpClient::Close() {
    LogInfof(logger_, "http close...");
    client_->Close();
}

void HttpClient::OnConnect(int ret_code) {
    if (ret_code < 0) {
        LogErrorf(logger_, "http client OnConnect error:%d", ret_code);
        std::shared_ptr<HttpClientResponse> resp_ptr;
        cb_->OnHttpRead(ret_code, resp_ptr);
        return;
    }
    std::stringstream http_stream;

    LogInfof(logger_, "on connect code:%d", ret_code);
    if (method_ == HTTP_GET) {
        http_stream << "GET " << subpath_ << " HTTP/1.1\r\n";
    } else if (method_ == HTTP_POST) {
        http_stream << "POST " << subpath_ << " HTTP/1.1\r\n";
    } else {
        CSM_THROW_ERROR("unkown http method:%d", method_);
    }
    http_stream << "Accept: */*\r\n";
    http_stream << "Host: " << host_ << "\r\n";
    for (auto& header : headers_) {
        http_stream << header.first << ": " << header.second << "\r\n";
    }
    if (method_ == HTTP_POST) {
        http_stream << "Content-Length: " << post_data_.length() << "\r\n";
    }
    http_stream << "\r\n";
    if (method_ == HTTP_POST) {
        http_stream << post_data_;
    }
    LogInfof(logger_, "http post:%s", http_stream.str().c_str());
    client_->Send(http_stream.str().c_str(), http_stream.str().length());
}

void HttpClient::OnWrite(int ret_code, size_t sent_size) {
    if (ret_code == 0) {
        client_->AsyncRead();
    }
}

void HttpClient::OnRead(int ret_code, const char* data, size_t data_size) {
    if (ret_code < 0) {
        //LogErrorf(logger_, "http client OnRead error:%d", ret_code);
        cb_->OnHttpRead(ret_code, resp_ptr_);
        return;
    }

    if (data_size == 0) {
        cb_->OnHttpRead(-2, resp_ptr_);
        return;
    }

    if (!resp_ptr_) {
        resp_ptr_ = std::make_shared<HttpClientResponse>();
    }

    resp_ptr_->data_.AppendData(data, data_size);

    if (!resp_ptr_->header_ready_) {
        std::string header_str(resp_ptr_->data_.Data(), resp_ptr_->data_.DataLen());
        size_t pos = header_str.find("\r\n\r\n");
        if (pos != std::string::npos) {
            std::vector<std::string> lines_vec;
            resp_ptr_->header_ready_ = true;
            header_str = header_str.substr(0, pos);
            resp_ptr_->data_.ConsumeData(pos + 4);

            StringSplit(header_str, "\r\n", lines_vec);
            for (size_t i = 0; i < lines_vec.size(); i++) {
                if (i == 0) {
                    std::vector<std::string> item_vec;
                    StringSplit(lines_vec[0], " ", item_vec);
                    assert(item_vec.size() >= 3);

                    pos = item_vec[0].find("/");
                    assert(pos != std::string::npos);
                    resp_ptr_->proto_   = item_vec[0].substr(0, pos);
                    resp_ptr_->version_ = item_vec[0].substr(pos+1);
                    resp_ptr_->status_code_ = atoi(item_vec[1].c_str());
                    if (item_vec.size() == 3) {
                        resp_ptr_->status_      = item_vec[2];
                    } else {
                        std::string status_string("");
                        for (size_t i = 2; i < item_vec.size(); i++) {
                            status_string += item_vec[i];
                        }
                        resp_ptr_->status_ = status_string;
                    }
                    continue;
                }
                pos = lines_vec[i].find(":");
                assert(pos != std::string::npos);
                std::string key   = lines_vec[i].substr(0, pos);
                std::string value = lines_vec[i].substr(pos + 1);

                if (key == "Content-Length") {
                    resp_ptr_->content_length_ = atoi(value.c_str());
                    LogInfof(logger_, "http content length:%d", resp_ptr_->content_length_);
                }
                resp_ptr_->headers_[key] = value;
                LogInfof(logger_, "header: %s: %s", key.c_str(), value.c_str());
            }
        } else {
            LogInfof(logger_, "header not ready, read more");
            client_->AsyncRead();
            return;
        }
    }

    if (resp_ptr_->content_length_ > 0) {
        LogInfof(logger_, "http receive data len:%lu, content len:%d",
                resp_ptr_->data_.DataLen(), resp_ptr_->content_length_);
        if ((int)resp_ptr_->data_.DataLen() >= resp_ptr_->content_length_) {
            resp_ptr_->body_ready_ = true;
            cb_->OnHttpRead(0, resp_ptr_);
        } else {
            client_->AsyncRead();
        }
    } else {
        cb_->OnHttpRead(0, resp_ptr_);
        client_->AsyncRead();
    }
}

}
