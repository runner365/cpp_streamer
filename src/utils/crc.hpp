#ifndef CRC_HPP
#define CRC_HPP

#include <stdint.h>
#include <stddef.h>

namespace cpp_streamer
{
    extern uint32_t CRC_32_IEEE[];
    extern uint32_t CRC_32_IEEE_LE[];
    uint32_t GetCrc32(uint32_t* crc_table, uint32_t crc, const uint8_t* data, size_t size);
}
#endif