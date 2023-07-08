#ifndef TCP_PUB_HPP
#define TCP_PUB_HPP
#include "data_buffer.hpp"
#include <uv.h>
#include <stdint.h>
#include <stddef.h>
#include <string>

namespace cpp_streamer
{

#define TCP_DEF_RECV_BUFFER_SIZE (5*1024)

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;

class TcpClientCallback
{
public:
    virtual void OnConnect(int ret_code) = 0;
    virtual void OnWrite(int ret_code, size_t sent_size) = 0;
    virtual void OnRead(int ret_code, const char* data, size_t data_size) = 0;
};

class TcpServerCallbackI
{
public:
    virtual void OnAccept(int ret_code, uv_loop_t* loop, uv_stream_t* handle) = 0;
};

class TcpSessionCallbackI
{
public:
    virtual void OnWrite(int ret_code, size_t sent_size) = 0;
    virtual void OnRead(int ret_code, const char* data, size_t data_size) = 0;
};

class TcpBaseSession
{
public:
    virtual void AsyncWrite(const char* data, size_t data_size) = 0;
    virtual void AsyncWrite(std::shared_ptr<DataBuffer> buffer_ptr) = 0;
    virtual void AsyncRead() = 0;
    virtual void Close() = 0;
    virtual std::string GetRemoteEndpoint() = 0;
    virtual std::string GetLocalEndpoint() = 0;
};

}

#endif
