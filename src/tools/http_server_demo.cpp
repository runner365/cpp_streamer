#include "http_server.hpp"
#include <stdint.h>
#include <string>
#include <iostream>
#include <memory>
#include <unistd.h>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <map>

using namespace cpp_streamer;

static Logger* s_logger = nullptr;

static void PostEcho(const HttpRequest* request, std::shared_ptr<HttpResponse> response_ptr);

int main(int argc, char** argv) {
    char log_file[516];

    int opt = 0;
    bool log_file_ready = false;
    char server_ip[80];
    bool server_ip_ready = false;
    char port_sz[32];
    uint16_t server_port = 0;
    char key_file[256];
    bool key_ready = false;
    char cert_file[256];
    bool cert_ready = false;

    while ((opt = getopt(argc, argv, "s:p:k:c:l:h")) != -1) {
        switch (opt) {
            case 's': strncpy(server_ip, optarg, sizeof(server_ip)); server_ip_ready = true; break;
            case 'p': strncpy(port_sz, optarg, sizeof(port_sz)); server_port = atoi(port_sz); break;
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'k': strncpy(key_file, optarg, sizeof(key_file)); key_ready = true; break;
            case 'c': strncpy(cert_file, optarg, sizeof(cert_file)); cert_ready = true; break;
            default: 
            {
                printf("Usage: %s [-s http server host ip]\n\
    [-p http server host port]\n\
    [-k https key file]\n\
    [-c https cert file]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!server_ip_ready) {
        sprintf(server_ip, "0.0.0.0");
    }
    if (server_port == 0) {
        std::cout << "please input server port.\r\n";
        return -1;
    }

    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
    }
    uv_loop_t* loop = uv_default_loop();

    try {
        std::unique_ptr<HttpServer> server_ptr;

        if (key_ready && cert_ready) {
            server_ptr.reset(new HttpServer(loop, server_port, key_file, cert_file, s_logger));
        } else {
            server_ptr.reset(new HttpServer(loop, server_port, s_logger));
        }
        
        server_ptr->AddPostHandle("/echo", PostEcho);
        
        while (true) {
            uv_run(loop, UV_RUN_DEFAULT);
        }
    } catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    
    return 0;
}

void PostEcho(const HttpRequest* request, std::shared_ptr<HttpResponse> response_ptr) {
    std::stringstream ss;
    ss << "---------- handle /api/v1/postdemo ------------" << std::endl;
    ss << "version:" << request->version_ << std::endl;
    ss << "method:" << request->method_ << std::endl;
    ss << "uri:" << request->uri_ << std::endl;
    ss << "content_length_:" << request->content_length_ << std::endl;

    for (const auto& header : request->headers_) {
        ss << header.first << ": " << header.second << std::endl;
    }

    std::string content_str(request->content_body_, request->content_length_);
    ss << "data body:\r\n" << content_str << std::endl;

    LogInfof(s_logger, "%s", ss.str().c_str());

    response_ptr->SetStatusCode(200);
    response_ptr->SetStatus("OK");
    response_ptr->Write(content_str.c_str(), content_str.length());
}