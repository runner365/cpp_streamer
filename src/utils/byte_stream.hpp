#ifndef BYTE_STREAM_HPP
#define BYTE_STREAM_HPP
#include <stdint.h>
#include <stddef.h>

namespace cpp_streamer
{
union AV_INT_FLOAT64 {
    uint64_t i;
    double   f;
};

class ByteStream
{
public:
    static double ByteInt2Double(uint64_t i) {
        union AV_INT_FLOAT64 v;
        v.i = i;
        return v.f;
    }
    static uint64_t ByteDouble2Int(double f) {
        union AV_INT_FLOAT64 v;
        v.f = f;
        return v.i;
    }
    
    static uint64_t Read8Bytes(const uint8_t* data) {
        uint64_t value = 0;
        uint8_t* output = (uint8_t*)&value;

        output[7] = *data++;
        output[6] = *data++;
        output[5] = *data++;
        output[4] = *data++;
        output[3] = *data++;
        output[2] = *data++;
        output[1] = *data++;
        output[0] = *data++;

        return value;
    }
    static uint64_t Read8BytesLe(const uint8_t* data) {
        uint64_t value = 0;
        uint8_t* output = (uint8_t*)&value;

        output[0] = *data++;
        output[1] = *data++;
        output[2] = *data++;
        output[3] = *data++;
        output[4] = *data++;
        output[5] = *data++;
        output[6] = *data++;
        output[7] = *data++;

        return value;
    }
    static uint32_t Read4Bytes(const uint8_t* data) {
        uint32_t value = 0;
        uint8_t* output = (uint8_t*)&value;

        output[3] = *data++;
        output[2] = *data++;
        output[1] = *data++;
        output[0] = *data++;

        return value;
    }
    static uint32_t Read4BytesLe(const uint8_t* data) {
        uint32_t value = 0;
        uint8_t* output = (uint8_t*)&value;

        output[0] = *data++;
        output[1] = *data++;
        output[2] = *data++;
        output[3] = *data++;

        return value;
    }
    static uint32_t Read3Bytes(const uint8_t* data) {
        uint32_t value = 0;
        uint8_t* output = (uint8_t*)&value;

        output[2] = *data++;
        output[1] = *data++;
        output[0] = *data++;

        return value;
    }
    static uint32_t Read3BytesLe(const uint8_t* data) {
        uint32_t value = 0;
        uint8_t* output = (uint8_t*)&value;

        output[0] = *data++;
        output[1] = *data++;
        output[2] = *data++;

        return value;
    }
    static uint16_t Read2Bytes(const uint8_t* data) {
        uint16_t value = 0;
        uint8_t* output = (uint8_t*)&value;

        output[1] = *data++;
        output[0] = *data++;

        return value;
    }
    static uint16_t Read2BytesLe(const uint8_t* data) {
        uint16_t value = 0;
        uint8_t* output = (uint8_t*)&value;

        output[0] = *data++;
        output[1] = *data++;

        return value;
    }
    static void Write8Bytes(uint8_t* data, uint64_t value) {
        uint8_t* p = data;
        uint8_t* pp = (uint8_t*)&value;

        *p++ = pp[7];
        *p++ = pp[6];
        *p++ = pp[5];
        *p++ = pp[4];
        *p++ = pp[3];
        *p++ = pp[2];
        *p++ = pp[1];
        *p++ = pp[0];
    }
    static void Write8Bytes_le(uint8_t* data, uint64_t value) {
        uint8_t* p = data;
        uint8_t* pp = (uint8_t*)&value;

        *p++ = pp[0];
        *p++ = pp[1];
        *p++ = pp[2];
        *p++ = pp[3];
        *p++ = pp[4];
        *p++ = pp[5];
        *p++ = pp[6];
        *p++ = pp[7];
    }
    static void Write4Bytes(uint8_t* data, uint32_t value) {
        uint8_t* p = data;
        uint8_t* pp = (uint8_t*)&value;

        *p++ = pp[3];
        *p++ = pp[2];
        *p++ = pp[1];
        *p++ = pp[0];
    }
    static void Write2Bytes_le(uint8_t* data, uint32_t value) {
        uint8_t* p = data;
        uint8_t* pp = (uint8_t*)&value;

        *p++ = pp[0];
        *p++ = pp[1];
    }
    static void Write4Bytes_le(uint8_t* data, uint32_t value) {
        uint8_t* p = data;
        uint8_t* pp = (uint8_t*)&value;

        *p++ = pp[0];
        *p++ = pp[1];
        *p++ = pp[2];
        *p++ = pp[3];
    }
    static void Write3Bytes(uint8_t* data, uint32_t value) {
        uint8_t* p = data;
        uint8_t* pp = (uint8_t*)&value;

        *p++ = pp[2];
        *p++ = pp[1];
        *p++ = pp[0];
    }
    static void Write2Bytes(uint8_t* data, uint16_t value) {
        uint8_t* p = data;
        uint8_t* pp = (uint8_t*)&value;

        *p++ = pp[1];
        *p++ = pp[0];
    }
    
    static bool BytesIsEqual(const char* p1, const char* p2, size_t len) {
        for (size_t index = 0; index < len; index++) {
            if (p1[index] != p2[index]) {
                return false;
            }
        }
        return true;
    }
    
    static uint16_t PadTo4Bytes(uint16_t size) {
        if (size & 0x03)
            return (size & 0xFFFC) + 4;
        else
            return size;
    }
    static uint32_t PadTo4Bytes(uint32_t size) {
        if (size & 0x03)
            return (size & 0xFFFFFFFC) + 4;
        else
            return size;
    }
};

}
#endif //BYTE_STREAM_HPP
