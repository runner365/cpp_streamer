#include "srtp_session.hpp"
#include "logger.hpp"
#include <vector>
#include <cstring>
#include <netinet/in.h>

namespace cpp_streamer
{

std::vector<const char*> SRtpSession::errors =
{
    // From 0 (srtp_err_status_ok) to 24 (srtp_err_status_pfkey_err).
    "success (srtp_err_status_ok)",
    "unspecified failure (srtp_err_status_fail)",
    "unsupported parameter (srtp_err_status_bad_param)",
    "couldn't allocate memory (srtp_err_status_alloc_fail)",
    "couldn't deallocate memory (srtp_err_status_dealloc_fail)",
    "couldn't initialize (srtp_err_status_init_fail)",
    "can’t process as much data as requested (srtp_err_status_terminus)",
    "authentication failure (srtp_err_status_auth_fail)",
    "cipher failure (srtp_err_status_cipher_fail)",
    "replay check failed (bad index) (srtp_err_status_replay_fail)",
    "replay check failed (index too old) (srtp_err_status_replay_old)",
    "algorithm failed test routine (srtp_err_status_algo_fail)",
    "unsupported operation (srtp_err_status_no_such_op)",
    "no appropriate context found (srtp_err_status_no_ctx)",
    "unable to perform desired validation (srtp_err_status_cant_check)",
    "can’t use key any more (srtp_err_status_key_expired)",
    "error in use of socket (srtp_err_status_socket_err)",
    "error in use POSIX signals (srtp_err_status_signal_err)",
    "nonce check failed (srtp_err_status_nonce_bad)",
    "couldn’t read data (srtp_err_status_read_fail)",
    "couldn’t write data (srtp_err_status_write_fail)",
    "error parsing data (srtp_err_status_parse_err)",
    "error encoding data (srtp_err_status_encode_err)",
    "error while using semaphores (srtp_err_status_semaphore_err)",
    "error while using pfkey (srtp_err_status_pfkey_err)"
};

bool SRtpSession::init_ = false;
Logger* SRtpSession::logger_ = nullptr;

void SRtpSession::Init(Logger* logger) {
    if (init_) {
        LogInfof(SRtpSession::logger_, "srtp session has been initialized.");
        return;
    }
    logger_ = logger;

    LogInfof(logger, "libsrtp version: <%s>", srtp_get_version_string());

    srtp_err_status_t err = srtp_init();
    if ((err != srtp_err_status_ok)) {
        CSM_THROW_ERROR("set srtp_init error: %s", SRtpSession::errors.at(err));
    }

    err = srtp_install_event_handler(static_cast<srtp_event_handler_func_t*>(SRtpSession::OnSRtpEvent));
    if ((err != srtp_err_status_ok)) {
        CSM_THROW_ERROR("set srtp_install_event_handler error: %s", SRtpSession::errors.at(err));
    }
    init_ = true;
    LogInfof(logger, "srtp session init ok...");
}

void SRtpSession::OnSRtpEvent(srtp_event_data_t* data) {
    switch (data->event)
    {
        case event_ssrc_collision:
            LogWarnf(SRtpSession::logger_, "SSRC collision occurred");
            break;

        case event_key_soft_limit:
            LogWarnf(SRtpSession::logger_, "stream reached the soft key usage limit and will expire soon");
            break;

        case event_key_hard_limit:
            LogWarnf(SRtpSession::logger_, "stream reached the hard key usage limit and has expired");
            break;

        case event_packet_index_limit:
            LogWarnf(SRtpSession::logger_, "stream reached the hard packet limit (2^48 packets)");
            break;
        default:
            LogErrorf(SRtpSession::logger_, "unkown srtp event:%d", data->event);
    }
}

SRtpSession::SRtpSession(SRTP_SESSION_TYPE session_type, CRYPTO_SUITE_ENUM suite, uint8_t* key, size_t key_len)
{
    srtp_policy_t policy;

    std::memset((void*)&policy, 0, sizeof(srtp_policy_t));

    std::string suite_desc;
    switch (suite) {
        case CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_80:
        {
            suite_desc = "CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_80";
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
            break;
        }
        case CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_32:
        {
            suite_desc = "CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_32";
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtcp);
            break;
        }
        case CRYPTO_SUITE_AEAD_AES_256_GCM:
        {
            suite_desc = "CRYPTO_SUITE_AEAD_AES_256_GCM";
            srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
            srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
            break;
        }
        case CRYPTO_SUITE_AEAD_AES_128_GCM:
        {
            suite_desc = "CRYPTO_SUITE_AEAD_AES_128_GCM";
            srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
            srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
            break;
        }
        default:
        {
            CSM_THROW_ERROR("unknown srtp crypto suite:%d", (int)suite);
        }
    }

    if ((int)key_len != policy.rtp.cipher_key_len) {
        CSM_THROW_ERROR("key length(%d) error, configure key length(%d)",
                (int)key_len, policy.rtp.cipher_key_len);
    }

    std::string session_desc;
    switch (session_type)
    {
        case SRTP_SESSION_IN_TYPE:
        {
            session_desc = "srtp read";
            policy.ssrc.type = ssrc_any_inbound;
            break;
        }
        case SRTP_SESSION_OUT_TYPE:
        {
            session_desc = "srtp write";
            policy.ssrc.type = ssrc_any_outbound;
            break;
        }
        default:
        {
            CSM_THROW_ERROR("unknown srtp session type:%d", (int)session_type);
        }
    }

    policy.ssrc.value      = 0;
    policy.key             = key;
    policy.allow_repeat_tx = 1;
    policy.window_size     = 8192;
    policy.next            = nullptr;

    srtp_err_status_t err = srtp_create(&session_, &policy);

    if (err != srtp_err_status_ok) {
        CSM_THROW_ERROR("srtp_create error: %s", SRtpSession::errors.at(err));
    LogInfof(SRtpSession::logger_, "srtp session construct, type:<%s>, suite:%s",
        session_desc.c_str(), suite_desc.c_str());
    }
}

SRtpSession::~SRtpSession() {
    LogInfof(logger_, "destruct SRtpSession");
    if (session_ != nullptr) {
        srtp_err_status_t err = srtp_dealloc(session_);
        if (err != srtp_err_status_ok)
            LogErrorf(SRtpSession::logger_, "srtp_dealloc error: %s", SRtpSession::errors.at(err));
    }
}

bool SRtpSession::EncryptRtp(uint8_t** data, size_t* len) {
    //in srtp.h
    //#define SRTP_MAX_TRAILER_LEN (SRTP_MAX_TAG_LEN + SRTP_MAX_MKI_LEN)
    //SRTP_MAX_TRAILER_LEN=-16+128
    if (*len + SRTP_MAX_TRAILER_LEN > SRTP_ENCRYPT_BUFFER_SIZE) {
        LogErrorf(SRtpSession::logger_, "fail to encrypt RTP packet, size too big (%lu bytes)", *len);
        return false;
    }

    std::memcpy(encrypt_buffer_, *data, *len);

    srtp_err_status_t err = srtp_protect(session_, (void*)(encrypt_buffer_), (int*)(len));

    if (err != srtp_err_status_ok) {
        LogErrorf(SRtpSession::logger_, "srtp_protect error: %s", SRtpSession::errors.at(err));
        return false;
    }

    *data = encrypt_buffer_;
    return true;
}

bool SRtpSession::DecryptSrtp(uint8_t* data, size_t* len) {
    srtp_err_status_t err = srtp_unprotect(session_, (void*)(data), (int*)(len));
    if (err != srtp_err_status_ok) {
        //LogErrorf(SRtpSession::logger_, "srtp_unprotect error: <%s>, data len:%lu", SRtpSession::errors.at(err), *len);
        return false;
    }
    return true;
}

bool SRtpSession::EncryptRtcp(uint8_t** data, size_t* len) {
    //#define SRTP_MAX_TRAILER_LEN (SRTP_MAX_TAG_LEN + SRTP_MAX_MKI_LEN)
    //SRTP_MAX_TRAILER_LEN=-16+128
    if (*len + SRTP_MAX_TRAILER_LEN > SRTP_ENCRYPT_BUFFER_SIZE) {
        LogErrorf(SRtpSession::logger_, "fail to encrypt RTP packet, size too big (%lu bytes)", *len);
        return false;
    }

    std::memcpy(encrypt_buffer_, *data, *len);

    srtp_err_status_t err = srtp_protect_rtcp(session_, (void*)(encrypt_buffer_), (int*)(len));

    if (err != srtp_err_status_ok) {
        LogErrorf(SRtpSession::logger_, "srtp_protect_rtcp error: %s", SRtpSession::errors.at(err));
        return false;
    }

    *data = encrypt_buffer_;
    return true;
}

bool SRtpSession::DecryptSrtcp(uint8_t* data, size_t* len) {
    srtp_err_status_t err = srtp_unprotect_rtcp(session_, (void*)(data), (int*)(len));
    if (err != srtp_err_status_ok) {
        //LogErrorf(SRtpSession::logger_, "srtp_unprotect_rtcp error: %s", SRtpSession::errors.at(err));
        return false;
    }
    return true;
}

void SRtpSession::RemoveStream(uint32_t ssrc) {
    srtp_remove_stream(session_, (uint32_t)(htonl(ssrc)));
    return;
}

}
