#ifndef SSL_PUB_HPP
#define SSL_PUB_HPP
#include <stdint.h>
#include <stddef.h>

namespace cpp_streamer
{

#define SSL_DEF_RECV_BUFFER_SIZE (10*1024)

typedef enum {
    TLS_SSL_SERVER_ZERO,
    TLS_SSL_SERVER_INIT_DONE      = 1,
    TLS_SSL_SERVER_HELLO_DONE     = 2,
    TLS_SERVER_KEY_EXCHANGE_DONE  = 3,
    TLS_SERVER_DATA_RECV_STATE    = 4
} TLS_SERVER_STATE;

typedef enum {
    TLS_SSL_CLIENT_ZERO,
    TLS_CLIENT_HELLO_DONE   = 1,
    TLS_CLIENT_KEY_EXCHANGE = 2,
    TLS_CLIENT_READY        = 3
} TLS_CLIENT_STATE;

class SslCallbackI
{
public:
    virtual void PlaintextDataSend(const char* data, size_t len) = 0;
    virtual void PlaintextDataRecv(const char* data, size_t len) = 0;
};


}

#endif

