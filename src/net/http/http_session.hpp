#ifndef HTTP_SESSION_HPP
#define HTTP_SESSION_HPP
#include "tcp_session.hpp"
#include "data_buffer.hpp"
#include "stringex.hpp"
#include "logger.hpp"
#include "session_aliver.hpp"

#include <stdint.h>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <assert.h>

namespace cpp_streamer
{
class HttpCallbackI;
class HttpRequest;
class HttpResponse;

class HttpSession : public TcpSessionCallbackI, public SessionAliver
{
friend class HttpResponse;

public:
    HttpSession(uv_loop_t* loop, uv_stream_t* handle, HttpCallbackI* callback, Logger* logger = nullptr);
    HttpSession(uv_loop_t* loop, uv_stream_t* handle, HttpCallbackI* callback,
                const std::string& key_file, const std::string& cert_file, Logger* logger);
    virtual ~HttpSession();

public:
    void TryRead();
    void Write(const char* data, size_t len);
    void Close();
    bool IsContinue() { return continue_flag_; }
    std::string RemoteEndpoint() { return remote_address_; }

protected://TcpSessionCallbackI
    virtual void OnWrite(int ret_code, size_t sent_size) override;
    virtual void OnRead(int ret_code, const char* data, size_t data_size) override;

private:
    int HandleRequest(const char* data, size_t data_size, bool& continue_flag);
    int AnalyzeHeader();

private:
    HttpCallbackI* callback_;
    Logger* logger_ = nullptr;
    std::shared_ptr<TcpBaseSession> session_ptr_;
    std::shared_ptr<HttpResponse> response_ptr_;
    DataBuffer header_data_;
    DataBuffer content_data_;
    HttpRequest* request_;
    int content_start_ = -1;

private:
    bool header_is_ready_ = false;
    bool is_closed_ = false;
    bool continue_flag_ = false;
    std::string remote_address_;
};

}
#endif
