#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H
#include "logger.hpp"
#include "tcp_pub.hpp"
#include "ssl_client.hpp"
#include "ipaddress.hpp"

#include <uv.h>
#include <memory>
#include <queue>
#include <string>
#include <stdint.h>
#include <sstream>
#include <netdb.h>

namespace cpp_streamer
{

inline void OnUVClientConnected(uv_connect_t *conn, int status);
inline void OnUVClientWrite(uv_write_t* req, int status);
inline void OnUVClientAlloc(uv_handle_t* handle,
                    size_t suggested_size,
                    uv_buf_t* buf);
inline void OnUVClientRead(uv_stream_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf);
inline void OnUVClose(uv_handle_t *handle) {}

class TcpClient : public SslCallbackI
{
friend void OnUVClientConnected(uv_connect_t *conn, int status);
friend void OnUVClientWrite(uv_write_t* req, int status);
friend void OnUVClientAlloc(uv_handle_t* handle,
                    size_t suggested_size,
                    uv_buf_t* buf);
friend void OnUVClientRead(uv_stream_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf);
public:
    TcpClient(uv_loop_t* loop,
        TcpClientCallback* callback,
        Logger* logger = nullptr,
        bool ssl_enable = false) : callback_(callback)
                                   , ssl_enable_(ssl_enable)
                                   , logger_(logger)
    {   
        client_  = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
        connect_ = (uv_connect_t*)malloc(sizeof(uv_connect_t));

        uv_tcp_init(loop, client_);

        buffer_ = (char*)malloc(buffer_size_);
        if (ssl_enable) {
            ssl_client_ = new SslClient(this, logger);
        }
    }

    virtual ~TcpClient() {
        Close();
        if (buffer_) {
            free(buffer_);
            buffer_ = nullptr;
        }
        if (ssl_client_) {
            delete ssl_client_;
            ssl_client_ = nullptr;
        }
        if (connect_) {
            uv_read_stop(connect_->handle);
            free(connect_);
            connect_ = nullptr;
        }
        if (client_) {
            uv_tcp_close_reset(client_, OnUVClose);
            free(client_);
            client_ = nullptr;
        }
    }

public:
    virtual void PlaintextDataSend(const char* data, size_t len) {
        char* new_data = (char*)malloc(len);
        memcpy(new_data, data, len);

        write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
        req->buf = uv_buf_init(new_data, len);

        connect_->handle->data = this;
        if (uv_write((uv_write_t*)req, connect_->handle, &req->buf, 1, OnUVClientWrite)) {
            free(new_data);
            throw CppStreamException("PlaintextDataSend uv_write error");
        }
        return;
 
    }

    virtual void PlaintextDataRecv(const char* data, size_t len) {
        callback_->OnRead(0, data, len);
    }

public:
    void Connect(const std::string& host, uint16_t dst_port) {
        int r = 0;
        char port_sz[80];
        std::string dst_ip;

        if (!IsIPv4(host)) {
            snprintf(port_sz, sizeof(port_sz), "%d", dst_port);
            addrinfo hints;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;

            addrinfo* ai  = NULL;
            LogInfof(logger_, "getaddrinfo host:%s, port:%s, ssl:%s", 
                    host.c_str(), port_sz, ssl_enable_ ? "true" : "false");
            if(getaddrinfo(host.c_str(), port_sz, (const addrinfo*)&hints, &ai)) {
                throw CppStreamException("get address info error");
            }
            assert(sizeof(dst_addr_) == ai->ai_addrlen);

            memcpy((void*)&dst_addr_, ai->ai_addr, sizeof(dst_addr_));
            dst_ip = GetIpStr(ai->ai_addr, dst_port);
            freeaddrinfo(ai);
        } else {
            GetIpv4Sockaddr(host, htons(dst_port), (struct sockaddr*)&dst_addr_);
            dst_ip = GetIpStr((sockaddr*)&dst_addr_, dst_port);
        }

        //if ((r = uv_ip4_addr(host.c_str(), dst_port, &dst_addr_)) != 0) {
        //    throw CppStreamException("connect address error");
        //}
        connect_->data = this;
        LogInfof(logger_, "start connect host:%s:%d", dst_ip.c_str(), htons(dst_port));

        if ((r = uv_tcp_connect(connect_, client_,
                            (const struct sockaddr*)&dst_addr_,
                            OnUVClientConnected)) != 0) {
            throw CppStreamException("connect address error");
        }
        return;
    }

    void Send(const char* data, size_t len) {
        if (ssl_enable_) {
            ssl_client_->SslWrite((uint8_t*)data, len);
            return;
        }
        char* new_data = (char*)malloc(len);
        memcpy(new_data, data, len);

        write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
        req->buf = uv_buf_init(new_data, len);

        connect_->handle->data = this;
        if (uv_write((uv_write_t*)req, connect_->handle, &req->buf, 1, OnUVClientWrite)) {
            free(new_data);
            throw CppStreamException("uv_write error");
        }
        return;
    }

    void AsyncRead() {
        if (!is_connect_) {
            return;
        }
        if (read_start_) {
            return;
        }
        read_start_ = true;
        int r = 0;

        if ((r = uv_read_start(connect_->handle, OnUVClientAlloc, OnUVClientRead)) != 0) {
            if (r == UV_EALREADY) {
                return;
            }
            std::stringstream err_ss;
            err_ss << "uv_read_start error:" << r;
            throw CppStreamException(err_ss.str().c_str());
        }
    }

    void Close() {
        if (!is_connect_) {
            return;
        }
        is_connect_ = false;
        connect_->handle->data = this;
        uv_read_stop(connect_->handle);
    }

    bool IsConnect() {
        return is_connect_;
    }

private:
    void OnConnect(int status) {
        if (status == 0) {
            is_connect_ = true;
        }
        LogInfof(logger_, "tcp connected ssl enable:%s", ssl_enable_ ? "true" : "false");
        if (!ssl_enable_) {
            if (callback_) {
                callback_->OnConnect(status);
            }
            return;
        }

        int ret = 0;
        TLS_CLIENT_STATE state = ssl_client_->GetState();
        if (state == TLS_SSL_CLIENT_ZERO) {
            ret = ssl_client_->ClientHello();
            if (ret < 0) {
                callback_->OnConnect(ret);
                return;
            }
        } else {
            LogErrorf(logger_, "tcp connected ssl state:%d error", state);
        }
    }

    void OnAlloc(uv_buf_t* buf) {
        buf->base = buffer_;
        buf->len  = buffer_size_;
    }

    void OnWrite(write_req_t* req, int status) {
        write_req_t* wr;
      
        if (ssl_enable_) {
            if (ssl_client_->GetState() < TLS_CLIENT_READY) {
                AsyncRead();
                return;
            }
        }
        /* Free the read/write buffer and the request */
        wr = (write_req_t*) req;
        if (callback_) {
            callback_->OnWrite(status, wr->buf.len);
        }
        free(wr->buf.base);
        free(wr);
    }

    void OnRead(ssize_t nread, const uv_buf_t* buf) {
        if (nread < 0) {
            callback_->OnRead(nread, nullptr, 0);
            return;
        } else if (nread == 0) {
            return;
        }
        if (!ssl_enable_) {
            callback_->OnRead(0, buf->base, nread);
            return;
        }
        TLS_CLIENT_STATE state = ssl_client_->GetState();
        int ret = 0;

        if (state == TLS_CLIENT_HELLO_DONE) {
            ret = ssl_client_->RecvServerHello(buf->base, nread);
            if (ret < 0) {
                callback_->OnConnect(ret);
            } else if (ret > 0) {
                AsyncRead();
            } else {
                LogInfof(logger_, "Ssl Client Hello Done");
            }
        } else if (state == TLS_CLIENT_KEY_EXCHANGE) {
            ret = ssl_client_->HandleSessionTicket(buf->base, nread);
            if (ret < 0) {
                callback_->OnConnect(ret);
            } else if (ret > 0) {
                AsyncRead();
            } else {
                LogInfof(logger_, "ssl client handshake done");
                callback_->OnConnect(0);
            }
        } else if (state == TLS_CLIENT_READY) {
            ssl_client_->HandleSslDataRecv((uint8_t*)buf->base, nread);
        } else {
            LogErrorf(logger_, "state error:%d", state);
            assert(false);
        }
        return;
    }

private:
    struct sockaddr_in dst_addr_;
    uv_tcp_t* client_            = nullptr;
    uv_connect_t* connect_       = nullptr;
    TcpClientCallback* callback_ = nullptr;
    
    char* buffer_       = nullptr;
    size_t buffer_size_ = 10*1024;
    bool is_connect_    = false;
    bool read_start_    = false;

private:
    bool ssl_enable_ = false;
    SslClient* ssl_client_ = nullptr;

private:
    Logger* logger_ = nullptr;
};

inline void OnUVClientConnected(uv_connect_t *conn, int status) {
    TcpClient* client = (TcpClient*)conn->data;
    if (client) {
        client->OnConnect(status);
    }
}

inline void OnUVClientWrite(uv_write_t* req, int status) {
    TcpClient* client = static_cast<TcpClient*>(req->handle->data);

    if (client) {
        client->OnWrite((write_req_t*)req, status);
    }
    return;
}

inline void OnUVClientAlloc(uv_handle_t* handle,
                    size_t suggested_size,
                    uv_buf_t* buf)
{
    TcpClient* client = (TcpClient*)handle->data;
    if (client) {
        client->OnAlloc(buf);
    }
}

inline void OnUVClientRead(uv_stream_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf)
{
    TcpClient* client = (TcpClient*)handle->data;
    if (client && client->IsConnect()) {
        client->OnRead(nread, buf);
    }
    return;
}

}
#endif //TCP_CLIENT_H

