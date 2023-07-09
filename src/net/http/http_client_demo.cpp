#include "http_client.hpp"
#include <sstream>
#include <uv.h>

using namespace cpp_streamer;

Logger* s_logger = nullptr;

class client : public HttpClientCallbackI
{
public:
    client(uv_loop_t* loop, const std::string& schema,
            const std::string& host, uint16_t port,
            Logger* logger = nullptr):schema_(schema)
                                      , host_(host)
                                      , port_(port)
                                      , loop_(loop)
                                      , logger_(logger)
    {
    }
    virtual ~client() {
        if (hc_) {
            delete hc_;
            hc_ = nullptr;
        }
    }

public:
    void Get(const std::string& subpath) {
        std::map<std::string, std::string> headers;
        if (hc_) {
            delete hc_;
            hc_ = nullptr;
        }
        if (schema_ == "http") {
            hc_ = new HttpClient(loop_, host_, port_, this, logger_);
        } else if (schema_ == "https") {
            hc_ = new HttpClient(loop_, host_, port_, this, logger_, true);
        } else {
            std::string err = "the schema error:";
            err += schema_;
            throw CppStreamException(err.c_str());
        }

        hc_->Get(subpath, headers);
    }

    void Post(const std::string& subpath, const std::string& data) {
        std::map<std::string, std::string> headers;
        if (hc_) {
            delete hc_;
            hc_ = nullptr;
        }
        if (schema_ == "http") {
            hc_ = new HttpClient(loop_, host_, port_, this, logger_);
        } else if (schema_ == "https") {
            hc_ = new HttpClient(loop_, host_, port_, this, logger_, true);
        } else {
            std::string err = "the schema error:";
            err += schema_;
            throw CppStreamException(err.c_str());
        }


        hc_->Post(subpath, headers, data);
    }

private:
    virtual void OnHttpRead(int ret, std::shared_ptr<HttpClientResponse> resp_ptr) override {
        if (ret < 0) {
            LogInfof(logger_, "OnHttpRead return error:%d", ret);
            if (hc_) {
                hc_->Close();
                delete hc_;
                hc_ = nullptr;
            }
            return;
        }
        std::string resp_data(resp_ptr->data_.Data(), resp_ptr->data_.DataLen());

        LogInfof(logger_, "http status:%d, status desc:%s, content len:%d",
                resp_ptr->status_code_, resp_ptr->status_.c_str(), resp_ptr->content_length_);
        LogInfof(logger_, "http response:%s", resp_data.c_str());
    }

private:
    std::string schema_ = "http";
    std::string host_;
    uint16_t port_ = 0;
    HttpClient* hc_ = nullptr;
    uv_loop_t* loop_ = nullptr;
    Logger* logger_ = nullptr;
};

int main(int argn, char** argv) {
    //uint16_t port = 6969;
    uint16_t port = 4443;
    std::string hostip = "127.0.0.1";

    uv_loop_t* loop = uv_default_loop();
    std::stringstream ss;

    s_logger = new Logger();
    s_logger->SetFilename(std::string("http_client_demo.log"));

    try {
        client c(loop, "https", hostip, port, s_logger);

        c.Get("/api/v1/producers/stat");

        //ss << "{\r\n";
        //ss << "\"uid\": 123456,\r\n";
        //ss << "\"age\": 23,\r\n";
        //ss << "}\r\n";
        //c.Post("/api/post/helloworld", ss.str());
        
        while(true) {
            uv_run(loop, UV_RUN_DEFAULT);
        }
    } catch(const std::exception& e) {
        std::cerr << "http server exception:" << e.what() << "\r\n";
    }
    
    LogInfof(s_logger, "loop done...");
    return 0;
}

