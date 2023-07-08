#ifndef DATA_BUFFER_H
#define DATA_BUFFER_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <string>
#include <vector>
#include <memory>

#define EXTRA_LEN (10*1024)

#define PRE_RESERVE_HEADER_SIZE 200

namespace cpp_streamer
{
class DataBuffer
{
public:
    DataBuffer(size_t data_size = EXTRA_LEN)
    {
        buffer_      = new char[data_size + PRE_RESERVE_HEADER_SIZE];
        buffer_size_ = data_size;
        start_       = PRE_RESERVE_HEADER_SIZE;
        end_         = PRE_RESERVE_HEADER_SIZE;
        data_len_    = 0;
        memset(buffer_, 0, data_size);
    }

    DataBuffer(const DataBuffer& input)//deep copy
    {
        sent_flag_     = input.sent_flag_;
        dst_ip_        = input.dst_ip_;
        dst_port_      = input.dst_port_;

        buffer_        = new char[input.buffer_size_ + PRE_RESERVE_HEADER_SIZE];
        buffer_size_   = input.buffer_size_;
        data_len_      = input.data_len_;
        start_         = input.start_;
        end_           = input.end_;

        memcpy(buffer_, input.buffer_, data_len_);
    }
    DataBuffer& operator=(const DataBuffer& input)//deep copy
    {
        sent_flag_     = input.sent_flag_;
        dst_ip_        = input.dst_ip_;
        dst_port_      = input.dst_port_;

        buffer_        = new char[input.buffer_size_ + PRE_RESERVE_HEADER_SIZE];
        buffer_size_   = input.buffer_size_;
        data_len_      = input.data_len_;
        start_         = input.start_;
        end_           = input.end_;

        memcpy(buffer_, input.buffer_, data_len_);
        return *this;
    }
    ~DataBuffer()
    {
        if (buffer_) {
            delete[] buffer_;
        }
    }

public:
    int AppendData(const char* input_data, size_t input_len) {
        if ((input_data == nullptr) || (input_len == 0)) {
            return 0;
        }
        if ((size_t)end_ + input_len > (buffer_size_ - PRE_RESERVE_HEADER_SIZE)) {
            if (data_len_ + input_len >= (buffer_size_ - PRE_RESERVE_HEADER_SIZE)) {
                int new_len = data_len_ + (int)input_len + EXTRA_LEN;

                new_len = GetNewSize(new_len);
                char* new_buffer = new char[new_len];
                memcpy(new_buffer + PRE_RESERVE_HEADER_SIZE, buffer_ + start_, data_len_);
                memcpy(new_buffer + PRE_RESERVE_HEADER_SIZE + data_len_, input_data, input_len);
                delete[] buffer_;
                buffer_      = new_buffer;
                buffer_size_ = new_len;
                data_len_    += input_len;
                start_     = PRE_RESERVE_HEADER_SIZE;
                end_       = start_ + data_len_;
                return data_len_;
            }
            if (data_len_ >= start_) {
                char* temp_p = new char[data_len_];
                memcpy(temp_p, buffer_ + start_, data_len_);
                memcpy(buffer_ + PRE_RESERVE_HEADER_SIZE, temp_p, data_len_);
                delete[] temp_p;
            } else {
                memcpy(buffer_ + PRE_RESERVE_HEADER_SIZE, buffer_ + start_, data_len_);
            }

            memcpy(buffer_ + PRE_RESERVE_HEADER_SIZE + data_len_, input_data, input_len);

            data_len_ += input_len;
            start_     = PRE_RESERVE_HEADER_SIZE;
            end_       = start_ + data_len_;
            return data_len_;
        }
        memcpy(buffer_ + end_, input_data, input_len);
        data_len_ += (int)input_len;
        end_ += (int)input_len;
        return data_len_;
    }

    char* ConsumeData(int consume_len) {
        if (consume_len > data_len_) {
            return nullptr;
        }

        if (consume_len < 0) {
            if ((start_ + consume_len) < 0) {
                return nullptr;
            }
        }
        start_    += consume_len;
        data_len_ -= consume_len;

        return buffer_ + start_;
    }
    void Reset() {
        start_    = PRE_RESERVE_HEADER_SIZE;
        end_      = PRE_RESERVE_HEADER_SIZE;
        data_len_ = 0;
    }

    char* Data() {
        return buffer_ + start_;
    }
    size_t DataLen() {
        return data_len_;
    }
    bool Require(size_t len) {
        if ((int)len <= data_len_) {
            return true;
        }
        return false;
    }

public:
    bool GetSentFlag() { return sent_flag_; }
    void SetSentFlag(bool flag) { sent_flag_ = flag; }
    std::string GetDstIp() { return dst_ip_; }
    void SetDstIp(const std::string& ip) { dst_ip_ = ip; }
    uint16_t GetDstPort() { return dst_port_; }
    void SetDstPort(uint16_t port) { dst_port_ = port; }

private:
    static int GetNewSize(int new_len) {
        int ret = new_len;
    
        if (new_len <= 50*1024) {
            ret = 50*1024;
        } else if ((new_len > 50*1024) && (new_len <= 100*1024)) {
            ret = 100*1024;
        } else if ((new_len > 100*1024) && (new_len <= 200*1024)) {
            ret = 200*1024;
        } else if ((new_len > 200*1024) && (new_len <= 500*1024)) {
            ret = 500*1024;
        } else {
            ret = new_len + 10*1024;
        }
        return ret;
    }

private:
    bool sent_flag_     = false;
    std::string dst_ip_;
    uint16_t    dst_port_ = 0;

private:
    char* buffer_       = nullptr;
    size_t buffer_size_ = 0;
    int data_len_       = 0;
    int start_          = PRE_RESERVE_HEADER_SIZE;
    int end_            = 0;
};

typedef std::shared_ptr<DataBuffer> DATA_BUFFER_PTR;

}
#endif //DATA_BUFFER_H
