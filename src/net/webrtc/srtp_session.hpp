#ifndef SRTP_SESSION_HPP
#define SRTP_SESSION_HPP
#include "logger.hpp"

#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <srtp.h>

namespace cpp_streamer
{
typedef enum
{
    CRYPTO_SUITE_NONE                    = 0,
    CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_80 = 1,
    CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_32,
    CRYPTO_SUITE_AEAD_AES_256_GCM,
    CRYPTO_SUITE_AEAD_AES_128_GCM
} CRYPTO_SUITE_ENUM;

typedef struct SRTP_CRYPTO_SUITE_ENTRY_S
{
    CRYPTO_SUITE_ENUM crypto_suite;
    const char* name;
} SRTP_CRYPTO_SUITE_ENTRY;

typedef enum
{
    SRTP_SESSION_IN_TYPE = 0,
    SRTP_SESSION_OUT_TYPE = 1
} SRTP_SESSION_TYPE;

#define SRTP_ENCRYPT_BUFFER_SIZE (10*1024)

class SRtpSession
{
public:
    SRtpSession(SRTP_SESSION_TYPE session_type, CRYPTO_SUITE_ENUM suite, uint8_t* key, size_t keyLen);
    ~SRtpSession();

public:
    static void Init(Logger* logger);
    static void OnSRtpEvent(srtp_event_data_t* data);

public:
    bool EncryptRtp(uint8_t** data, size_t* len);
    bool DecryptSrtp(uint8_t* data, size_t* len);
    bool EncryptRtcp(uint8_t** data, size_t* len);
    bool DecryptSrtcp(uint8_t* data, size_t* len);
    void RemoveStream(uint32_t ssrc);

private:
    static Logger* logger_;
    static bool init_;
    static std::vector<const char*> errors;

private:
    srtp_t session_ = nullptr;

private:
    uint8_t encrypt_buffer_[SRTP_ENCRYPT_BUFFER_SIZE];
};

}

#endif
