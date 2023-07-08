#ifndef STUN_HPP
#define STUN_HPP
#include "ipaddress.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string>

namespace cpp_streamer
{
#define STUN_HEADER_SIZE 20
/*
rfc: https://datatracker.ietf.org/doc/html/rfc5389
All STUN messages MUST start with a 20-byte header followed by zero
or more Attributes.  The STUN header contains a STUN message type,
magic cookie, transaction ID, and message length.
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |0 0|     STUN Message Type     |         Message Length        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         Magic Cookie                          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |                     Transaction ID (96 bits)                  |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
typedef enum
{
    STUN_REQUEST          = 0,
    STUN_INDICATION       = 1,
    STUN_SUCCESS_RESPONSE = 2,
    STUN_ERROR_RESPONSE   = 3
} STUN_CLASS_ENUM;

typedef enum
{
    BINDING = 1
} STUN_METHOD_ENUM;


typedef enum
{
    STUN_MAPPED_ADDRESS     = 0x0001,
    STUN_USERNAME           = 0x0006,
    STUN_MESSAGE_INTEGRITY  = 0x0008,
    STUN_ERROR_CODE         = 0x0009,
    STUN_UNKNOWN_ATTRIBUTES = 0x000A,
    STUN_REALM              = 0x0014,
    STUN_NONCE              = 0x0015,
    STUN_XOR_MAPPED_ADDRESS = 0x0020,
    STUN_PRIORITY           = 0x0024,
    STUN_USE_CANDIDATE      = 0x0025,
    STUN_SOFTWARE           = 0x8022,
    STUN_ALTERNATE_SERVER   = 0x8023,
    STUN_FINGERPRINT        = 0x8028,
    STUN_ICE_CONTROLLED     = 0x8029,
    STUN_ICE_CONTROLLING    = 0x802A
} STUN_ATTRIBUTE_ENUM;

typedef enum
{
    OK           = 0,
    UNAUTHORIZED = 1,
    BAD_REQUEST  = 2
} STUN_AUTHENTICATION;

class StunPacket
{
public:
    uint8_t data_[8192];
    size_t data_len_ = 0;
public:
    static const uint8_t magic_cookie[];

public:
    STUN_CLASS_ENUM  stun_class_;
    STUN_METHOD_ENUM stun_method_;
    uint8_t transaction_id_[12];
    const uint8_t* message_integrity_ = nullptr;

public:
    std::string username_;
    std::string password_;
    uint32_t fingerprint_     = 0;
    uint32_t priority_        = 0;
    uint64_t ice_controlling_ = 0;
    uint64_t ice_controlled_  = 0;
    uint16_t error_code_      = 0;
    struct sockaddr* xor_address_ = nullptr; // 8 or 20 bytes. 
    bool has_use_candidate_ = false;
    bool add_msg_integrity_ = false;
    bool has_fingerprint_   = false;

public:
    StunPacket();
    StunPacket(const uint8_t* data, size_t len);
    ~StunPacket();

public:
    static bool IsStun(const uint8_t* data, size_t len);
    static bool IsBindingRequest(const uint8_t *buf, size_t buf_size);
    static bool IsBindingResponse(const uint8_t *buf, size_t buf_size);
    static StunPacket* Parse(const uint8_t* data, size_t len);

/*
need to initialize:
1) username_: 
snprintf(username, sizeof(username), "%s:%s",
      dtls->remote_fragment_, rtc->local_fragment_);
2) add_msg_integrity_, dtls->remote_pwd_ for password_;
ByteCrypto::GetHmacSha1(password_,...
*/
    int Serialize();
    std::string Dump();
};

}

#endif
