#include "udp_server.hpp"
#include "logger.hpp"

#include <iostream>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <unistd.h>
#include <chrono>

using namespace cpp_streamer;

class EchoCallback;

static Logger s_logger;
static EchoCallback* echo_cb = nullptr;
static UdpServer* server = nullptr;

class EchoCallback : public UdpSessionCallbackI
{
public:
    EchoCallback()
    {

    }
    virtual ~EchoCallback()
    {

    }

public:
    virtual void OnWrite(size_t sent_size, UdpTuple address) {

    }

    virtual void OnRead(const char* data, size_t data_size, UdpTuple address) {
        //LogInfof(&s_logger, "receive data len:%u, from address:%s", 
        //        data_size, address.to_string().c_str());
        server->Write(data, data_size, address);
    }
};

int main(int argc, char** argv) {
    uint16_t udp_port = 0;
    char log_file[128];
    bool log_file_ready = false;
    int opt = 0;

    while ((opt = getopt(argc, argv, "p:l:h")) != -1) {
        switch (opt) {
            case 'p': udp_port = atoi(optarg); break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-p udp server]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }
    if (udp_port == 0) {
        std::cout << "udp port must be inputed.\r\n";
        return -1;
    }
    if (log_file_ready) {
        s_logger.SetFilename(log_file);
    }

    uv_loop_t* loop = uv_default_loop();
    echo_cb = new EchoCallback();
    server  = new UdpServer(loop, udp_port, echo_cb, &s_logger);

    try {
        LogInfof(&s_logger, "server is starting port:%d", udp_port);
        while (true) {
            uv_run(loop, UV_RUN_DEFAULT);
        }
    } catch (CppStreamException& e) {
        std::cout << "exception:" << e.what() << "\r\n";
    }

    delete server;
    delete echo_cb;
    uv_loop_close(loop);
    return 0;
}

