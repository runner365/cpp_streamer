#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP
#include "http_common.hpp"
#include "tcp_client.hpp"
#include "tcp_pub.hpp"
#include "data_buffer.hpp"
#include "logger.hpp"
#include <string>
#include <memory>
#include <map>
#include <uv.h>

namespace cpp_streamer
{

typedef enum {
    HTTP_GET,
    HTTP_POST
} HTTP_METHOD;

class HttpClientResponse
{
public:
    std::string status_  = "OK";  // e.g. "OK"
    int status_code_     = 200;     // e.g. 200
    std::string proto_   = "HTTP";   // e.g. "HTTP"
    std::string version_ = "1.1"; // e.g. "1.1"
    std::map<std::string, std::string> headers_; //headers
    int content_length_  = 0;
    DataBuffer data_;
    bool header_ready_ = false;
    bool body_ready_   = false;
};

class HttpClientCallbackI
{
public:
    virtual void OnHttpRead(int ret, std::shared_ptr<HttpClientResponse> resp_ptr) = 0;
};

class HttpClient : public TcpClientCallback
{
public:
    HttpClient(uv_loop_t* loop, const std::string& host, uint16_t port,
            HttpClientCallbackI* cb, Logger* logger = nullptr, bool ssl_enable = false);
    virtual ~HttpClient();

public:
    int Get(const std::string& subpath, std::map<std::string, std::string> headers);
    int Post(const std::string& subpath, std::map<std::string, std::string> headers, const std::string& data);
    void Close();
    
private:
    virtual void OnConnect(int ret_code) override;
    virtual void OnWrite(int ret_code, size_t sent_size) override;
    virtual void OnRead(int ret_code, const char* data, size_t data_size) override;

private:
    TcpClient* client_ = nullptr;
    std::string host_;
    uint16_t port_ = 0;
    HTTP_METHOD method_ = HTTP_GET;
    std::map<std::string, std::string> headers_;
    std::string subpath_;
    HttpClientCallbackI* cb_ = nullptr;
    std::string post_data_;
    std::shared_ptr<HttpClientResponse> resp_ptr_;

private:
    Logger* logger_ = nullptr;
};

}
#endif
