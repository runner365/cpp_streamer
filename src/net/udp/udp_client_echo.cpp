#include "udp_client.hpp"
#include "logger.hpp"
#include "timer.hpp"
#include "timeex.hpp"
#include "byte_stream.hpp"

#include <iostream>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <unistd.h>
#include <chrono>
#include <thread>

using namespace cpp_streamer;
class ClientSessionCallback;

static Logger s_logger;
static UdpClient* s_client = nullptr;
static ClientSessionCallback* s_callback = nullptr;

class ClientSessionCallback : public UdpSessionCallbackI, public TimerInterface
{
public:
    ClientSessionCallback(uv_loop_t* loop,
                        int interval_ms,
                        int count,
                        const std::string& ip,
                        uint16_t udp_port):TimerInterface(loop, interval_ms)
                                , count_(count)
                                , remote_address_(ip, udp_port)
    {
    }
    virtual ~ClientSessionCallback()
    {
    }

public:
    virtual void OnTimer() override {
        uint8_t data[1024];
        uint32_t now_ms = (uint32_t)now_millisec();

        memset(data, 0, sizeof(data));
        LogDebugf(&s_logger, "Write to:%s", remote_address_.to_string().c_str());
        for (int i = 0; i < count_; i++) {
            ByteStream::Write4Bytes(data, now_ms);
            ByteStream::Write4Bytes(data + 4, seq_++);

            s_client->Write((char*)data, sizeof(data), remote_address_);
        }
    }
    
public:
    virtual void OnWrite(size_t sent_size, UdpTuple address) override {
        LogDebugf(&s_logger, "written send size:%lu, remote:%s", 
                sent_size, address.to_string().c_str());
        s_client->TryRead();
    }
    virtual void OnRead(const char* data, size_t data_size, UdpTuple address) override {
        int64_t now_ms = (uint32_t)now_millisec();

        uint8_t* p = (uint8_t*)data;
        int64_t ts = (int64_t)ByteStream::Read4Bytes(p);
        uint32_t seq_ = ByteStream::Read4Bytes(p + 4);

        int64_t rtt_ms = now_ms - ts;
        rtt_ms_ = rtt_ms_ + (rtt_ms - rtt_ms_)/8;
        
        if (last_ms_ == 0) {
            last_ms_ = now_ms;
        } else {
            if ((now_ms - last_ms_) > 2000) {
                last_ms_ = now_ms;
                LogInfof(&s_logger, "receive rtt:%ld, seq:%u", rtt_ms_, seq_);
            }
        }
    }

private:
    int count_ = 0;
    uint32_t seq_ = 0;
    UdpTuple remote_address_;
    int64_t rtt_ms_ = 200;

private:
    int64_t last_ms_ = 0;
};

int main(int argc, char** argv) {
    uint16_t udp_port = 0;
    char serverip[32];
    char log_file[128];
    bool log_file_ready = false;
    bool serverip_ready = false;
    int opt = 0;

    while ((opt = getopt(argc, argv, "s:p:l:h")) != -1) {
        switch (opt) {
            case 's': strncpy(serverip, optarg, sizeof(serverip)); serverip_ready = true; break;
            case 'p': udp_port = atoi(optarg); break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'h':
            default: 
            {
                printf("Usage: %s [-s udp server ip]\n\
    [-p udp server port]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }
    if (!serverip_ready) {
        std::cout << "server ip must be configured.\r\n";
        return -1;
    }
    if (udp_port == 0) {
        std::cout << "server port must be configured.\r\n";
        return -1;
    }

    if (log_file_ready) {
        s_logger.SetFilename(log_file);
    }
    uv_loop_t* loop = uv_default_loop();

    s_callback = new ClientSessionCallback(loop,
                        10,
                        1000,
                        serverip,
                        udp_port);
    s_client = new UdpClient(loop, s_callback, &s_logger);
    
    try {
        s_callback->StartTimer();
        LogInfof(&s_logger, "client is starting: %s:%d",
                serverip, udp_port);
        while (true) {
            uv_run(loop, UV_RUN_DEFAULT);
        }
    } catch (CppStreamException& e) {
        std::cout << "exception:" << e.what() << "\r\n";
    }
    uv_loop_close(loop);
    return 0;
}
