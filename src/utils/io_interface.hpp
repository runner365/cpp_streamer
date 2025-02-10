#ifndef IO_INTERFACE_HPP
#define IO_INTERFACE_HPP
#include <stdint.h>
#include <stddef.h>

namespace cpp_streamer
{

class IoReaderI
{
public:
    virtual int IoRead(uint8_t*& data, size_t len) = 0;
};

class IoWriterI
{
public:
    virtual int IoWrite(const uint8_t* data, size_t len) = 0;
};

}
#endif