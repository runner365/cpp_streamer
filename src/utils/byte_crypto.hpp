#ifndef BYTE_CRYTO_HPP
#define BYTE_CRYTO_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <random>
#include <openssl/hmac.h>
#include <openssl/ssl.h>

namespace cpp_streamer
{
#define SHA1_BUFFER_SIZE 20

class ByteCrypto
{
public:
    static void Init();
    static void DeInit();
    static uint32_t GetRandomUint(uint32_t min, uint32_t max);
    static uint32_t GetCrc32(const uint8_t* data, size_t size);
    static uint32_t GetCrc32(uint32_t crc, const uint8_t* data, size_t size);
    static uint8_t* GetHmacSha1(const std::string& key, const uint8_t* data, size_t len);
    static std::string GetRandomString(size_t len);

public:
    static std::default_random_engine random;
    static HMAC_CTX* hmac_sha1_ctx;
    static uint8_t hmac_sha1_buffer[20];
    static const uint32_t crc32_table[256];

private:
    static bool init_;
};

}
#endif
