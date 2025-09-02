#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP
#include "net/tcp/tcp_server.hpp"
#include "utils/timer.hpp"
#include "net/http/http_common.hpp"
#include "net/http/http_session.hpp"
#include "utils/logger.hpp"
#include <memory>

namespace cpp_streamer
{
class HttpServer : public TcpServerCallbackI, public TimerInterface, public HttpCallbackI
{
public:
    HttpServer(uv_loop_t* loop, uint16_t port, Logger* logger);
    HttpServer(uv_loop_t* loop, uint16_t port, const std::string& key_file, const std::string& cert_file, Logger* logger);
    virtual ~HttpServer();

public:
    void AddGetHandle(std::string uri, HTTP_HANDLE_PTR handle_func);
    void AddPostHandle(std::string uri, HTTP_HANDLE_PTR handle_func);

public:// tcp callback
    virtual void OnAccept(int ret_code, uv_loop_t* loop, uv_stream_t* handle) override;

public:
    virtual void OnClose(const std::string& endpoint) override;
    virtual HTTP_HANDLE_PTR GetHandle(HttpRequest* request) override;

public:
    virtual void OnTimer() override;

private:
    std::shared_ptr<TcpServer> server_;
    bool ssl_enable_ = false;
    std::string key_file_;
    std::string cert_file_;
    std::unordered_map< std::string, std::shared_ptr<HttpSession> > session_ptr_map_;
    std::unordered_map< std::string, HTTP_HANDLE_PTR > get_handle_map_;
    std::unordered_map< std::string, HTTP_HANDLE_PTR > post_handle_map_;
private:
    Logger* logger_ = nullptr;
};
}
#endif