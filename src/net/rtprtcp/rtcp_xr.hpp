#ifndef RTCP_XR_HPP
#define RTCP_XR_HPP
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t  bt;
    uint8_t  reserver;
    uint16_t block_length;
} XrCommonData;


#endif

