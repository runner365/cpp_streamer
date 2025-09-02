#include "http_server.hpp"

namespace cpp_streamer
{
HttpServer::HttpServer(uv_loop_t* loop, uint16_t port, Logger* logger):TimerInterface(loop, 3000)
                                                                    , logger_(logger)
{
    server_ = std::make_shared<TcpServer>(loop, port, this);
    ssl_enable_ = false;
    StartTimer();
}

HttpServer::HttpServer(uv_loop_t* loop, 
    uint16_t port, 
    const std::string& key_file, 
    const std::string& cert_file,
    Logger* logger):TimerInterface(loop, 3000)
                            , key_file_(key_file)
                            , cert_file_(cert_file)
                            , logger_(logger)
{
    server_     = std::make_shared<TcpServer>(loop, port, this);
    ssl_enable_ = true;
    StartTimer();
}
HttpServer::~HttpServer()
{
    StopTimer();
}

void HttpServer::AddGetHandle(std::string uri, HTTP_HANDLE_PTR handle_func) {
    std::string uri_key = GetUri(uri);
    get_handle_map_.insert(std::make_pair(uri_key, handle_func));
}

void HttpServer::AddPostHandle(std::string uri, HTTP_HANDLE_PTR handle_func) {
    std::string uri_key = GetUri(uri);
    post_handle_map_.insert(std::make_pair(uri_key, handle_func));
}

void HttpServer::OnAccept(int ret_code, uv_loop_t* loop, uv_stream_t* handle) {
    if (ret_code < 0) {
        return;
    }
    std::shared_ptr<HttpSession> session_ptr;
    if (ssl_enable_) {
        session_ptr = std::make_shared<HttpSession>(loop, handle, this, key_file_, cert_file_, logger_);
    } else {
        session_ptr = std::make_shared<HttpSession>(loop, handle, this, logger_);
    }

    session_ptr_map_.insert(std::make_pair(session_ptr->RemoteEndpoint(), session_ptr));
}


void HttpServer::OnClose(const std::string& endpoint) {
    auto iter = session_ptr_map_.find(endpoint);
    if (iter != session_ptr_map_.end()) {
        session_ptr_map_.erase(iter);
    }
}

HTTP_HANDLE_PTR HttpServer::GetHandle(HttpRequest* request) {
    HTTP_HANDLE_PTR handle_func = nullptr;
    std::unordered_map< std::string, HTTP_HANDLE_PTR >::iterator iter;
    GetUri(request->uri_);

    if (request->method_ == "GET") {
        iter = get_handle_map_.find(request->uri_);
        if (iter != get_handle_map_.end()) {
            handle_func = iter->second;
            return handle_func;
        }
    } else if (request->method_ == "POST") {
        iter = post_handle_map_.find(request->uri_);
        if (iter != post_handle_map_.end()) {
            handle_func = iter->second;
            return handle_func;
        }
    }
    
    iter = get_handle_map_.find("/");
    if (iter != get_handle_map_.end()) {
        handle_func = iter->second;
        return handle_func;
    }
    
    iter = post_handle_map_.find("/");
    if (iter != post_handle_map_.end()) {
        handle_func = iter->second;
        return handle_func;
    }
    
    return handle_func;
}

void HttpServer::OnTimer() {

}

}