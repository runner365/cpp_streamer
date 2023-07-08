#ifndef AFM0_HPP
#define AFM0_HPP
#include "byte_stream.hpp"
#include "data_buffer.hpp"

#include <stdint.h>
#include <vector>
#include <string>
#include <map>
#include <assert.h>

namespace cpp_streamer
{
typedef enum {
    AMF_DATA_TYPE_UNKNOWN     = -1,
    AMF_DATA_TYPE_NUMBER      = 0x00,
    AMF_DATA_TYPE_BOOL        = 0x01,
    AMF_DATA_TYPE_STRING      = 0x02,
    AMF_DATA_TYPE_OBJECT      = 0x03,
    AMF_DATA_TYPE_NULL        = 0x05,
    AMF_DATA_TYPE_UNDEFINED   = 0x06,
    AMF_DATA_TYPE_REFERENCE   = 0x07,
    AMF_DATA_TYPE_MIXEDARRAY  = 0x08,
    AMF_DATA_TYPE_OBJECT_END  = 0x09,
    AMF_DATA_TYPE_ARRAY       = 0x0a,
    AMF_DATA_TYPE_DATE        = 0x0b,
    AMF_DATA_TYPE_LONG_STRING = 0x0c,
    AMF_DATA_TYPE_UNSUPPORTED = 0x0d,
} AMF_DATA_TYPE;

class AMF_ITERM
{
public:
    AMF_ITERM() {}
    AMF_ITERM(const AMF_ITERM& input) = delete;
    AMF_ITERM& operator=(const AMF_ITERM& input) = delete;

    ~AMF_ITERM() {
        for (auto iter : amf_obj_) {
            AMF_ITERM* temp = iter.second;
            delete temp;
        }
        amf_obj_.clear();

        for (auto iter : amf_array_) {
            AMF_ITERM* temp = iter;
            delete temp;
        }
        amf_array_.clear();
    }

public:
    AMF_DATA_TYPE GetAmfType() {
        return amf_type_;
    }

    void SetAmfType(AMF_DATA_TYPE type) {
        amf_type_ = type;
    }

    std::string DumpAmf() {
        std::stringstream ss;
        switch (amf_type_)
        {
            case AMF_DATA_TYPE_NUMBER:
            {
                ss << "amf type: number, value:" <<  number_;
                break;
            }
            case AMF_DATA_TYPE_BOOL:
            {
                ss << "amf type: bool, value:" << enable_;
                break;
            }
            case AMF_DATA_TYPE_STRING:
            {
                ss << "amf type: string, value:" << desc_str_;
                break;
            }
            case AMF_DATA_TYPE_OBJECT:
            {
                ss << "amf type: object, count:" << amf_obj_.size() << "\r\n";
                for (auto iter : amf_obj_) {
                    ss << "object key:" << iter.first.c_str() << "\r\n";
                    ss << iter.second->DumpAmf() << "\r\n";
                }
                break;
            }
            case AMF_DATA_TYPE_NULL:
            {
                ss << "amf type: null";
                break;
            }
            case AMF_DATA_TYPE_UNDEFINED:
            {
                ss << "amf type: undefined";
                break;
            }
            case AMF_DATA_TYPE_REFERENCE:
            {
                ss << "not support for reference";
                break;
            }
            case AMF_DATA_TYPE_MIXEDARRAY:
            {
                ss << "amf type: ecma array, count:" << amf_obj_.size() << "\r\n";
                for (auto iter : amf_obj_) {
                    ss << "object key:" <<  iter.first.c_str()  << "\r\n";
                    ss << iter.second->DumpAmf() << "\r\n";
                }
                break;
            }
            case AMF_DATA_TYPE_ARRAY:
            {
                ss << "amf type: strict array, count:" <<  amf_array_.size() << "\r\n";
                for (auto iter : amf_array_) {
                    ss << iter->DumpAmf() << "\r\n";
                }
                break;
            }
            case AMF_DATA_TYPE_DATE:
            {
                ss << "amf type: date, number:" << number_;
                break;
            }
            case AMF_DATA_TYPE_LONG_STRING:
            {
                ss << "amf type: long string, string:" <<  desc_str_;
                break;
            }
            default:
                break;
        }
        return ss.str();
    }

public:
    AMF_DATA_TYPE amf_type_ = AMF_DATA_TYPE_UNKNOWN;

public:
    double number_ = 0.0;
    bool enable_   = false;
    std::string desc_str_;
    std::map<std::string, AMF_ITERM*> amf_obj_;
    std::vector<AMF_ITERM*> amf_array_;
};

class AMF_Encoder
{
public:
    static int Encode(double num, DataBuffer& buffer) {
        const size_t amf_len = 1 + 8;
        uint8_t data[amf_len];
        uint8_t* p     = data;

        *p = (uint8_t)AMF_DATA_TYPE_NUMBER;
        p++;

        uint64_t number = ByteStream::ByteDouble2Int(num);
        ByteStream::Write8Bytes(p, number);
        p += 8;

        buffer.AppendData((char*)data, amf_len);
        return 0;
    }

    static int EncodeNull(DataBuffer& buffer) {
        const size_t amf_len = 1;
        uint8_t data[amf_len];
        data[0] = AMF_DATA_TYPE_NULL;

        buffer.AppendData((char*)data, amf_len);
        return 0;
    }

    static int Encode(bool flag, DataBuffer& buffer) {
        const size_t amf_len = 1 + 1;
        uint8_t data[amf_len];
        data[0] = AMF_DATA_TYPE_BOOL;
        data[1] = flag ? 0x01 : 0x00;

        buffer.AppendData((char*)data, amf_len);
        return 0;
    }

    static int Encode(const std::string& str, DataBuffer& buffer, bool skip_marker = false) {
        if (str.length() > 0xffff) {
            uint32_t str_len = str.length();
            size_t amf_len   = 4 + str_len;
            uint8_t* data    = new uint8_t[2*amf_len];
            uint8_t* p       = data;

            if (!skip_marker) {
                *p = (uint8_t)AMF_DATA_TYPE_LONG_STRING;
                p++;
                amf_len++;
            }
            ByteStream::Write4Bytes(p, str_len);
            p += 4;
            memcpy(p, str.c_str(), str_len);

            buffer.AppendData((char*)data, amf_len);
            delete[] data;
        } else {
            uint16_t str_len = str.length();
            size_t amf_len   = 2 + str_len;
            uint8_t* data    = new uint8_t[2*amf_len];
            uint8_t* p       = data;

            if (!skip_marker) {
                *p = (uint8_t)AMF_DATA_TYPE_STRING;
                p++;
                amf_len++;
            }
            ByteStream::Write2Bytes(p, str_len);
            p += 2;
            if (str_len > 0) {
                memcpy(p, str.c_str(), str_len);
            }

            buffer.AppendData((char*)data, amf_len);
            delete[] data;
        }
        return 0;
    }

    static int EncodeOnlyType(AMF_DATA_TYPE amf_type, DataBuffer& buffer) {
        uint8_t data = (uint8_t)amf_type;

        buffer.AppendData((char*)&data, 1);
        return 0;
    }

    static int Encode(const std::map<std::string, AMF_ITERM*>& amf_obj, DataBuffer& buffer) {
        uint8_t start = AMF_DATA_TYPE_OBJECT;
        buffer.AppendData((char*)&start, 1);

        for (const auto& iter : amf_obj) {
            std::string key = iter.first;
            AMF_ITERM* amf_item = iter.second;

            AMF_Encoder::Encode(key, buffer, true);

            switch(amf_item->GetAmfType()) {
                case AMF_DATA_TYPE_NUMBER:
                {
                    AMF_Encoder::Encode(amf_item->number_, buffer);
                    break;
                }
                case AMF_DATA_TYPE_BOOL:
                {
                    AMF_Encoder::Encode(amf_item->enable_, buffer);
                    break;
                }
                case AMF_DATA_TYPE_STRING:
                {
                    AMF_Encoder::Encode(amf_item->desc_str_, buffer);
                    break;
                }
                case AMF_DATA_TYPE_NULL:
                {
                    AMF_Encoder::EncodeOnlyType(AMF_DATA_TYPE_NULL, buffer);
                    break;
                }
                case AMF_DATA_TYPE_UNDEFINED:
                {
                    AMF_Encoder::EncodeOnlyType(AMF_DATA_TYPE_UNDEFINED, buffer);
                    break;
                }
                case AMF_DATA_TYPE_REFERENCE:
                {
                    return -1;
                }
                case AMF_DATA_TYPE_MIXEDARRAY:
                {
                    //TODO: implement
                    break;
                }
                case AMF_DATA_TYPE_ARRAY:
                {
                    //TODO: implement
                    break;
                }
                case AMF_DATA_TYPE_DATE:
                {
                    //TODO: implement
                    break;
                }
                case AMF_DATA_TYPE_LONG_STRING:
                {
                    AMF_Encoder::Encode(amf_item->desc_str_, buffer);
                    break;
                }
                case AMF_DATA_TYPE_UNSUPPORTED:
                {
                    return -1;
                }
                default:
                    return -1;
            }
        }
        std::string end_str;
        AMF_Encoder::Encode(end_str, buffer, true);
        uint8_t end = AMF_DATA_TYPE_OBJECT_END;
        buffer.AppendData((char*)&end, 1);
        return 0;
    }
};

class AMF_Decoder
{
public:
    static int Decode(uint8_t*& data, int& left_len, AMF_ITERM& amf_item) {
        uint8_t type;
        AMF_DATA_TYPE amf_type;

        type = data[0];
        if ((type > AMF_DATA_TYPE_UNSUPPORTED) || (type < AMF_DATA_TYPE_NUMBER)) {
            return -1;
        }
        data++;
        left_len--;

        amf_type = (AMF_DATA_TYPE)type;
        switch (amf_type) {
            case AMF_DATA_TYPE_NUMBER:
            {
                uint64_t value = ByteStream::Read8Bytes(data);

                amf_item.SetAmfType(amf_type);
                amf_item.number_ = ByteStream::ByteInt2Double(value);

                data += 8;
                left_len -= 8;
                break;
            }
            case AMF_DATA_TYPE_BOOL:
            {
                uint8_t value = *data;

                amf_item.SetAmfType(amf_type);
                amf_item.enable_ = (value != 0) ? true : false;

                data++;
                left_len--;
                break;
            }
            case AMF_DATA_TYPE_STRING:
            {
                uint16_t str_len = ByteStream::Read2Bytes(data);
                data += 2;
                left_len -= 2;

                std::string desc((char*)data, str_len);

                amf_item.SetAmfType(amf_type);
                amf_item.desc_str_ = desc;

                data += str_len;
                left_len -= str_len;
                break;
            }
            case AMF_DATA_TYPE_OBJECT:
            {
                amf_item.SetAmfType(amf_type);
                int ret = DecodeAmfObject(data, left_len, amf_item.amf_obj_);
                if (ret != 0) {
                    return ret;
                }
                break;
            }
            case AMF_DATA_TYPE_NULL:
            {
                amf_item.SetAmfType(amf_type);
                break;
            }
            case AMF_DATA_TYPE_UNDEFINED:
            {
                amf_item.SetAmfType(amf_type);
                break;
            }
            case AMF_DATA_TYPE_REFERENCE:
            {
                assert(0);
                return -1;
            }
            case AMF_DATA_TYPE_MIXEDARRAY:
            {
                uint32_t array_len = ByteStream::Read4Bytes(data);
                data += 4;
                left_len -= 4;
                (void)array_len;

                amf_item.SetAmfType(AMF_DATA_TYPE_OBJECT);
                int ret = DecodeAmfObject(data, left_len, amf_item.amf_obj_);
                if (ret != 0) {
                    return ret;
                }
                break;
            }
            case AMF_DATA_TYPE_ARRAY:
            {
                amf_item.SetAmfType(AMF_DATA_TYPE_ARRAY);
                DecodeAmfArray(data, left_len, amf_item.amf_array_);
                break;
            }
            case AMF_DATA_TYPE_DATE:
            {
                amf_item.SetAmfType(AMF_DATA_TYPE_DATE);
                uint64_t value = ByteStream::Read8Bytes(data);

                amf_item.number_ = ByteStream::ByteInt2Double(value);

                data += 8;
                left_len -= 8;

                data += 2;
                left_len -= 2;
                break;
            }
            case AMF_DATA_TYPE_LONG_STRING:
            {
                uint32_t str_len = ByteStream::Read4Bytes(data);
                data += 4;
                left_len -= 4;

                std::string desc((char*)data, str_len);

                amf_item.SetAmfType(AMF_DATA_TYPE_LONG_STRING);
                amf_item.desc_str_ = desc;

                data += str_len;
                left_len -= str_len;
                break;
            }
            case AMF_DATA_TYPE_UNSUPPORTED:
            {
                amf_item.SetAmfType(AMF_DATA_TYPE_LONG_STRING);
                break;
            }
            default:
            {
                amf_item.SetAmfType(AMF_DATA_TYPE_UNKNOWN);
                return -1;
            }
        }

        return 0;
    }

    static int DecodeAmfObject(uint8_t*& data, int& len, std::map<std::string, AMF_ITERM*>& amf_obj) {
        //amf object: <key: string type> : <value: amf object type>
        
        while (len > 0) {
            uint16_t key_len = ByteStream::Read2Bytes(data);
            data += 2;
            len -= 2;

            if (key_len == 0) {
                assert((AMF_DATA_TYPE)data[0] == AMF_DATA_TYPE_OBJECT_END);
                break;
            }
            std::string key((char*)data, key_len);
            data += key_len;
            len -= key_len;

            if ((data[0] > AMF_DATA_TYPE_UNSUPPORTED) || (data[0] < AMF_DATA_TYPE_NUMBER)) {
                return -1;
            }
            AMF_ITERM* amf_item = new AMF_ITERM();
            int ret = Decode(data, len, *amf_item);
            if (ret != 0) {
                return ret;
            }
            amf_obj.insert(std::make_pair(key, amf_item));

        }
        return 0;
    }

    static int DecodeAmfArray(uint8_t*& data, int& len, std::vector<AMF_ITERM*>& amf_array) {
        uint32_t array_len = ByteStream::Read4Bytes(data);
        data += 4;
        len -= 4;

        for (uint32_t index = 0; index < array_len; index++) {
            AMF_ITERM* item = new AMF_ITERM();
            int ret = Decode(data, len, *item);
            if (ret != 0) {
                return ret;
            }
            amf_array.push_back(item);
        }
        return 0;
    }
};

}
#endif

