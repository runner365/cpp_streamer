#ifndef TCP_SESSION_BASE_H
#define TCP_SESSION_BASE_H
#include "logger.hpp"
#include "data_buffer.hpp"
#include "tcp_pub.hpp"
#include "ipaddress.hpp"
#include "ssl_server.hpp"
#include <uv.h>
#include <memory>
#include <string>
#include <stdint.h>
#include <iostream>
#include <stdio.h>
#include <queue>
#include <sstream>
#include <openssl/ssl.h>
#include <assert.h>

namespace cpp_streamer
{

inline static void OnTcpClose(uv_handle_t* handle);
inline static void OnUvAlloc(uv_handle_t* handle,
                       size_t suggested_size,
                       uv_buf_t* buf);
inline static void OnUvRead(uv_stream_t* handle,
                       ssize_t nread,
                       const uv_buf_t* buf);
inline static void OnUvWrite(uv_write_t* req, int status);

class TcpSession : public TcpBaseSession, public SslCallbackI
{
friend void OnTcpClose(uv_handle_t* handle);
friend void OnUvAlloc(uv_handle_t* handle,
                    size_t suggested_size,
                    uv_buf_t* buf);
friend void OnUvRead(uv_stream_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf);
friend void OnUvWrite(uv_write_t* req, int status);

public:
    TcpSession(uv_loop_t* loop,
            uv_stream_t* server_uv_handle,
            TcpSessionCallbackI* callback,
            Logger* logger):callback_(callback)
                            , logger_(logger)
    {
        buffer_    = (char*)malloc(buffer_size_);
        uv_handle_ = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
        uv_handle_->data = this;

        // Set the UV handle.
        int err = uv_tcp_init(loop, uv_handle_);
        if (err != 0) {
            delete uv_handle_;
            uv_handle_ = nullptr;
            free(buffer_);
            throw CppStreamException("uv_tcp_init() failed");
        }
        err = uv_accept(
          reinterpret_cast<uv_stream_t*>(server_uv_handle),
          reinterpret_cast<uv_stream_t*>(uv_handle_));
    
        if (err != 0) {
            delete uv_handle_;
            uv_handle_ = nullptr;
            free(buffer_);
            throw CppStreamException("uv_accept() failed");
        }
        int namelen = (int)sizeof(local_name_);
	    uv_tcp_getsockname(uv_handle_, &local_name_, &namelen);
        uv_tcp_getpeername(uv_handle_, &peer_name_, &namelen);
        close_ = false;
    }

    TcpSession(uv_loop_t* loop,
            uv_stream_t* server_uv_handle,
            TcpSessionCallbackI* callback,
            const std::string& key_file,
            const std::string& cert_file,
            Logger* logger):callback_(callback)
                           , ssl_enable_(true)
                           , ssl_(new SslServer(key_file, cert_file, this, logger))
                           , logger_(logger)
    {
        buffer_    = (char*)malloc(buffer_size_);
        uv_handle_ = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
        uv_handle_->data = this;

        // Set the UV handle.
        int err = uv_tcp_init(loop, uv_handle_);
        if (err != 0) {
            delete uv_handle_;
            uv_handle_ = nullptr;
            free(buffer_);
            throw CppStreamException("uv_tcp_init() failed");
        }
        err = uv_accept(
          reinterpret_cast<uv_stream_t*>(server_uv_handle),
          reinterpret_cast<uv_stream_t*>(uv_handle_));
    
        if (err != 0) {
            delete uv_handle_;
            uv_handle_ = nullptr;
            free(buffer_);
            throw CppStreamException("uv_accept() failed");
        }
        int namelen = (int)sizeof(local_name_);
        uv_tcp_getsockname(uv_handle_, &local_name_, &namelen);
        uv_tcp_getpeername(uv_handle_, &peer_name_, &namelen);
        close_ = false;
    }
    virtual ~TcpSession()
    {
        Close();
        if (ssl_) {
            delete ssl_;
            ssl_ = nullptr;
        }
        if (buffer_) {
            free(buffer_);
        }
    }

public:
    virtual void AsyncRead() override {
        if (close_) {
            return;
        }

        if (!uv_handle_) {
            return;
        }
    
        int err = uv_read_start(
                            reinterpret_cast<uv_stream_t*>(uv_handle_),
                            static_cast<uv_alloc_cb>(OnUvAlloc),
                            static_cast<uv_read_cb>(OnUvRead));
    
        if (err != 0) {
            if (err == UV_EALREADY) {
                return;
            }
            throw CppStreamException("uv_read_start() failed");
        }
    }

    virtual void AsyncWrite(const char* data, size_t len) override {
        if (ssl_enable_ && ssl_) {
            ssl_->SslWrite((uint8_t*)data, len);
            return;
        }
        write_req_t* wr = (write_req_t*) malloc(sizeof(write_req_t));

        char* new_data = (char*)malloc(len);
        memcpy(new_data, data, len);

        wr->buf = uv_buf_init(new_data, len);
        if (uv_write((uv_write_t*)wr, reinterpret_cast<uv_stream_t*>(uv_handle_), &wr->buf, 1, OnUvWrite)) {
            free(new_data);
            throw CppStreamException("uv_write error");
        }
    }

    virtual void AsyncWrite(std::shared_ptr<DataBuffer> buffer_ptr) override {
        this->AsyncWrite(buffer_ptr->Data(), buffer_ptr->DataLen());
    }

    virtual void Close() override {
        if (close_) {
            return;
        }
        close_ = true;

        int err = uv_read_stop(reinterpret_cast<uv_stream_t*>(uv_handle_));
        if (err != 0) {
            throw CppStreamException("uv_read_stop error");
        }
        LogDebugf(logger_, "tcp close");
        uv_close(reinterpret_cast<uv_handle_t*>(uv_handle_), static_cast<uv_close_cb>(OnTcpClose));
    }

    virtual std::string GetRemoteEndpoint() override {
        std::stringstream ss;
        uint16_t remoteport = 0;
        std::string remoteip = GetIpStr(&peer_name_, remoteport);

        ss << remoteip << ":" << remoteport;
        return ss.str();
    }

    virtual std::string GetLocalEndpoint() override {
        std::stringstream ss;
        uint16_t localport = 0;
        std::string localip = GetIpStr(&local_name_, localport);

        ss << localip << ":" << localport;
        return ss.str();
    }

private:
    virtual void PlaintextDataSend(const char* data, size_t len) override {
        write_req_t* wr = (write_req_t*) malloc(sizeof(write_req_t));

        char* new_data = (char*)malloc(len);
        memcpy(new_data, data, len);

        wr->buf = uv_buf_init(new_data, len);
        if (uv_write((uv_write_t*)wr, reinterpret_cast<uv_stream_t*>(uv_handle_), &wr->buf, 1, OnUvWrite)) {
            free(new_data);
            throw CppStreamException("uv_write error");
        }
    }

    virtual void PlaintextDataRecv(const char* data, size_t len) override {
        callback_->OnRead(0, data, len);
    }

    void OnAlloc(uv_buf_t* buf) {
        buf->base = buffer_;
        buf->len  = buffer_size_;
    }

    void OnRead(ssize_t nread, const uv_buf_t* buf) {
        if (close_) {
            return;
        }
        if (nread == 0) {
            return;
        }
        if (nread < 0) {
            callback_->OnRead(-1, nullptr, 0);
            return;
        }

        if (ssl_enable_) {
            int ret = ssl_->Handshake(buf->base, nread);
            if (ret != 0) {
                Close();
                return;
            }
            ssl_->HandleSslDataRecv((uint8_t*)buf->base, nread);
            AsyncRead();
            return;
        }

        callback_->OnRead(0, buf->base, nread);
    }

    void OnWrite(write_req_t* req, int status) {
        write_req_t* wr;
      
        /* Free the read/write buffer and the request */
        wr = (write_req_t*) req;
        if (ssl_enable_ && ssl_) {
            if (ssl_->GetState() == TLS_SERVER_DATA_RECV_STATE) {
                if (callback_ && !close_) {
                    callback_->OnWrite(status, wr->buf.len);
                }
            }
        } else {
            if (callback_ && !close_) {
                callback_->OnWrite(status, wr->buf.len);
            }
        }

        free(wr->buf.base);
        free(wr);
    }

private:
    TcpSessionCallbackI* callback_ = nullptr;
    uv_tcp_t* uv_handle_ = nullptr;
    struct sockaddr local_name_;
    struct sockaddr peer_name_;
    char* buffer_        = nullptr;
    size_t buffer_size_  = TCP_DEF_RECV_BUFFER_SIZE;
    bool close_          = false;

private:
    bool ssl_enable_     = false;
    SslServer* ssl_     = nullptr;

private:
    Logger* logger_;
};

inline static void OnUvAlloc(uv_handle_t* handle,
                       size_t suggested_size,
                       uv_buf_t* buf) {
    TcpSession* session = (TcpSession*)handle->data;
    if (session) {
        session->OnAlloc(buf);
    }
}

inline static void OnUvRead(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    TcpSession* session = (TcpSession*)handle->data;
    if (session) {
        if (nread == 0) {
            return;
        }
        session->OnRead(nread, buf);
    }
    return;
}

inline static void OnUvWrite(uv_write_t* req, int status) {
    TcpSession* session = static_cast<TcpSession*>(req->handle->data);

    if (session) {
        session->OnWrite((write_req_t*)req, status);
    }
    return;
}

inline static void OnTcpClose(uv_handle_t* handle) {
    delete handle;
}

}
#endif //TCP_SESSION_BASE_H
