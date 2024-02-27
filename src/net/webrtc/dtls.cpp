#include "dtls.hpp"
#include "byte_crypto.hpp"
#include "logger.hpp"
#include "timeex.hpp"
#include "peerconnection.hpp"

#include <assert.h>
#include <stdio.h>
#include <cstring>
#include <openssl/ec.h>

namespace cpp_streamer
{
std::vector<srtp_crypto_suite_map> srtp_crypto_suite_vec =
{
    { AEAD_AES_256_GCM, "SRTP_AEAD_AES_256_GCM" },
    { AEAD_AES_128_GCM, "SRTP_AEAD_AES_128_GCM" },
    { AES_CM_128_HMAC_SHA1_80, "SRTP_AES128_CM_SHA1_80" },
    { AES_CM_128_HMAC_SHA1_32, "SRTP_AES128_CM_SHA1_32" }
};

std::map<std::string, finger_print_algorithm_enum> string2finger_print_algorithm =
{
    { "sha-1",   FINGER_SHA1   },
    { "sha-224", FINGER_SHA224 },
    { "sha-256", FINGER_SHA256 },
    { "sha-384", FINGER_SHA384 },
    { "sha-512", FINGER_SHA512 }
};

std::map<finger_print_algorithm_enum, std::string> finger_print_algorithm2String =
{
    { FINGER_SHA1,   std::string("sha-1")   },
    { FINGER_SHA224, std::string("sha-224") },
    { FINGER_SHA256, std::string("sha-256") },
    { FINGER_SHA384, std::string("sha-384") },
    { FINGER_SHA512, std::string("sha-512") }
};

std::vector<SRTP_CRYPTO_SUITE_ENTRY> srtp_crypto_suites =
{
    { CRYPTO_SUITE_AEAD_AES_256_GCM, "SRTP_AEAD_AES_256_GCM" },
    { CRYPTO_SUITE_AEAD_AES_128_GCM, "SRTP_AEAD_AES_128_GCM" },
    { CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_80, "SRTP_AES128_CM_SHA1_80" },
    { CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_32, "SRTP_AES128_CM_SHA1_32" }
};



int on_ssl_certificate_verify(int, X509_STORE_CTX*) {
    //printf("ssl certificate verify callback: enable\r\n");
    return 1;
}

static const char* openssl_get_error(RtcDtls *ctx) {
    int r2 = ERR_get_error();
    if (r2)
        ERR_error_string_n(r2, ctx->error_message, sizeof(ctx->error_message));
    else
        ctx->error_message[0] = '\0';

    ERR_clear_error();
    return ctx->error_message;
}

static int openssl_ssl_get_error(RtcDtls *ctx, int ret) {
    SSL *dtls = ctx->dtls_;
    int r1 = SSL_ERROR_NONE;

    if (ret <= 0)
        r1 = SSL_get_error(dtls, ret);

    openssl_get_error(ctx);
    return r1;
}

void ssl_info_callback(const SSL* dtls, int where, int r0) {
    int w, r1, is_fatal, is_warning, is_close_notify;
    const char *method = "undefined", *alert_type, *alert_desc;
    enum DTLSState state;
    RtcDtls *ctx = (RtcDtls*)SSL_get_ex_data(dtls, 0);
    Logger* logger = ctx->logger_;

    w = where & ~SSL_ST_MASK;
    if (w & SSL_ST_CONNECT)
        method = "SSL_connect";
    else if (w & SSL_ST_ACCEPT)
        method = "SSL_accept";

    r1 = SSL_get_error(ctx->dtls_, r0);

    if (where & SSL_CB_LOOP) {
        LogDebugf(logger, "DTLS: method=%s", method);
        //LogInfof(logger, "DTLS: method=%s state=%s(%s), where=%d, ret=%d, r1=%d",
        //    method, SSL_state_string(dtls), SSL_state_string_long(dtls), where, r0, r1);
    } else if (where & SSL_CB_ALERT) {
        method = (where & SSL_CB_READ) ? "read":"write";

        alert_type = SSL_alert_type_string_long(r0);
        alert_desc = SSL_alert_desc_string(r0);
        LogInfof(logger, "dtls alert type:%s, alert_desc:%s", alert_type, alert_desc);

        if (!strcmp(alert_type, "warning") && !strcmp(alert_desc, "CN")) {
            LogInfof(logger, "DTLS: SSL3 alert method=%s type=%s, desc=%s(%s), where=%d, ret=%d, r1=%d",
                method, alert_type, alert_desc, SSL_alert_desc_string_long(r0), where, r0, r1);
        } else {
            LogInfof(logger, "DTLS: SSL3 alert method=%s type=%s, desc=%s(%s), where=%d, ret=%d, r1=%d",
                method, alert_type, alert_desc, SSL_alert_desc_string_long(r0), where, r0, r1);
        }

        /**
         * Notify the DTLS to handle the ALERT message, which maybe means media connection disconnect.
         * CN(Close Notify) is sent when peer close the PeerConnection. fatal, IP(Illegal Parameter)
         * is sent when DTLS failed.
         */
        is_fatal = !strcmp(alert_type, "fatal");
        is_warning = !strcmp(alert_type, "warning");
        is_close_notify = !strcmp(alert_desc, "CN");
        state = is_fatal ? DTLS_STATE_FAILED : (is_warning && is_close_notify ? DTLS_STATE_CLOSED : DTLS_STATE_NONE);
        if (state != DTLS_STATE_NONE) {
            LogInfof(logger, "DTLS: Notify ctx=%p, state=%d, fatal=%d, warning=%d, cn=%d",
                ctx, state, is_fatal, is_warning, is_close_notify);
            ctx->OnState(state, alert_type, alert_desc);
        }
    } else if (where & SSL_CB_EXIT) {
        if (!r0) {
            LogInfof(logger, "DTLS: Fail method=%s state=%s(%s), where=%d, ret=%d, r1=%d",
                method, SSL_state_string(dtls), SSL_state_string_long(dtls), where, r0, r1);
        }
        else if (r0 < 0) {
            if (r1 != SSL_ERROR_NONE && r1 != SSL_ERROR_WANT_READ && r1 != SSL_ERROR_WANT_WRITE) {
                LogInfof(logger, "DTLS: Error method=%s state=%s(%s), where=%d, ret=%d, r1=%d",
                    method, SSL_state_string(dtls), SSL_state_string_long(dtls), where, r0, r1);
            } else {
                LogInfof(logger, "DTLS: method=%s state=%s(%s), where=%d, ret=%d, r1=%d",
                    method, SSL_state_string(dtls), SSL_state_string_long(dtls), where, r0, r1);
            }
        }
    }
}

static void DtlsStateTrace(RtcDtls *ctx, uint8_t *data, int length, int incoming)
{
    uint8_t content_type = 0;
    uint16_t size = 0;
    uint8_t handshake_type = 0;
    Logger* logger = ctx->logger_;

    /* Change_cipher_spec(20), alert(21), handshake(22), application_data(23) */
    if (length >= 1)
        content_type = (uint8_t)data[0];
    if (length >= 13)
        size = (uint16_t)(data[11])<<8 | (uint16_t)data[12];
    if (length >= 14)
        handshake_type = (uint8_t)data[13];

    LogInfof(logger, "WHIP: DTLS state %s %s, done=%u, arq=%u, len=%u, cnt=%u, size=%u, hs=%u",
        "Active", (incoming? "RECV":"SEND"), ctx->dtls_done_for_us_, ctx->dtls_arq_packets_, length,
        content_type, size, handshake_type);
}

/**
 * DTLS BIO read callback.
 */
#if OPENSSL_VERSION_NUMBER < 0x30000000L // v3.0.x
static long openssl_dtls_bio_out_callback(BIO* b, int oper, const char* argp, int argi, long argl, long retvalue)
#else
static long openssl_dtls_bio_out_callback_ex(BIO *b, int oper, const char *argp, size_t len, int argi, long argl, int retvalue, size_t *processed)
#endif
{
    int ret, req_size = argi, is_arq = 0;
    uint8_t content_type, handshake_type;
    uint8_t *data = (uint8_t*)argp;
    RtcDtls* ctx = b ? (RtcDtls*)BIO_get_callback_arg(b) : NULL;
    Logger* logger = ctx ? ctx->logger_ : NULL;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L // v3.0.x
    req_size = len;
    LogDebugf(logger, "DTLS: bio callback b=%p, oper=%d, argp=%p, len=%ld, argi=%d, argl=%ld, retvalue=%d, processed=%p, req_size=%d\n",
        b, oper, argp, len, argi, argl, retvalue, processed, req_size);
#else
    LogDebugf(logger, "DTLS: bio callback b=%p, oper=%d, argp=%p, argi=%d, argl=%ld, retvalue=%ld, req_size=%d\n",
        b, oper, argp, argi, argl, retvalue, req_size);
#endif

    if (oper != BIO_CB_WRITE || !argp || req_size <= 0) {
        LogDebugf(logger, "bio out callback return, oper:%d, argp:%p, req_size:%d, BIO_CB_WRITE:%d",
                oper, argp, req_size, BIO_CB_WRITE);
        return retvalue;
    }

    DtlsStateTrace(ctx, data, req_size, 0);

    ret = ctx->OnWrite(data, req_size);
    content_type = req_size > 0 ? data[0] : 0;
    handshake_type = req_size > 13 ? data[13] : 0;

    is_arq = ctx->dtls_last_content_type_ == content_type && ctx->dtls_last_handshake_type_ == handshake_type;
    ctx->dtls_arq_packets_ += is_arq;
    ctx->dtls_last_content_type_ = content_type;
    ctx->dtls_last_handshake_type_ = handshake_type;

    if (ret < 0) {
        LogErrorf(logger, "DTLS: Send request failed, oper=%d, content=%d, handshake=%d, size=%d, is_arq=%d\n",
            oper, content_type, handshake_type, req_size, is_arq);
        return ret;
    }

    return retvalue;
}

RtcDtls::RtcDtls(PeerConnection* pc, Logger* logger):logger_(logger),
    pc_(pc)
{
    for (auto& item : srtp_crypto_suite_vec) {
        if (!srtp_ciphers_.empty()) {
            srtp_ciphers_ += ":";
        }
        srtp_ciphers_ += item.name;
    }

    local_fragment_ = ByteCrypto::GetRandomString(16);
    local_pwd_      = ByteCrypto::GetRandomString(32);

    fg_algorithm_ = "sha-256";
    LogInfof(logger_, "fragment:%s, user pwd:%s.\r\n",
            local_fragment_.c_str(), local_pwd_.c_str());
}

RtcDtls::~RtcDtls()
{
    LogInfof(logger_, "destruct RtcDtls");
    if(dtls_pkey_) {
        EVP_PKEY_free(dtls_pkey_);
        dtls_pkey_ = nullptr;
    }
    if (dtls_eckey_) {
        EC_KEY_free(dtls_eckey_);
        dtls_eckey_ = nullptr;
    }
    if (dtls_cert_) {
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
    }
}

int RtcDtls::GenPrivateKey() {
    LogInfof(logger_, "openssl version:%08x", OPENSSL_VERSION_NUMBER);
#if OPENSSL_VERSION_NUMBER < 0x30000000L /* OpenSSL 3.0 */
    EC_GROUP *ecgroup = NULL;
#else
    const char *curve = "prime256v1";
#endif

    /* Should use the curves in ClientHello.supported_groups, for example:
     *      Supported Group: x25519 (0x001d)
     *      Supported Group: secp256r1 (0x0017)
     *      Supported Group: secp384r1 (0x0018)
     * Note that secp256r1 in openssl is called NID_X9_62_prime256v1 or prime256v1 in string,
     * not NID_secp256k1 or secp256k1 in string
     */
#if OPENSSL_VERSION_NUMBER < 0x30000000L /* OpenSSL 3.0 */
    dtls_pkey_  = EVP_PKEY_new();
    dtls_eckey_ = EC_KEY_new();
    ecgroup     = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);

#if OPENSSL_VERSION_NUMBER < 0x10100000L // v1.1.x
    /* For openssl 1.0, we must set the group parameters, so that cert is ok. */
    EC_GROUP_set_asn1_flag(ecgroup, OPENSSL_EC_NAMED_CURVE);
#endif

    if (EC_KEY_set_group(dtls_eckey_, ecgroup) != 1) {
        LogErrorf(logger_, "DTLS: EC_KEY_set_group failed\n");
        EC_KEY_free(dtls_eckey_);
        dtls_eckey_ = nullptr;
        EC_GROUP_free(ecgroup);
        ecgroup = nullptr;
        return -1;
    }

    if (EC_KEY_generate_key(dtls_eckey_) != 1) {
        LogErrorf(logger_, "DTLS: EC_KEY_generate_key failed\n");
        EC_KEY_free(dtls_eckey_);
        dtls_eckey_ = nullptr;
        EC_GROUP_free(ecgroup);
        return -1;
    }

    if (EVP_PKEY_set1_EC_KEY(dtls_pkey_, dtls_eckey_) != 1) {
        LogErrorf(logger_, "DTLS: EVP_PKEY_set1_EC_KEY failed\n");
        EVP_PKEY_free(dtls_pkey_);
        dtls_pkey_ = nullptr;
        EC_KEY_free(dtls_eckey_);
        dtls_eckey_ = nullptr;
        EC_GROUP_free(ecgroup);
        return -1;
    }
    EC_KEY_free(dtls_eckey_);
    EC_GROUP_free(ecgroup);
#else
    dtls_pkey_ = EVP_EC_gen(curve);
    if (!dtls_pkey_) {
        LogErrorf(logger_, "DTLS: EVP_EC_gen curve=%s failed\n", curve);
        return -1;
    }
#endif

    return 0;
}

int RtcDtls::SslContextInit() {
    /* Generate a private key to ctx->dtls_pkey. */
    if (GenPrivateKey() < 0) {
        return -1;
    }

    if (GenPrivateCert() < 0) {
        return -1;
    }

    if (InitContext() < 0) {
        return -1;
    }

    rtc_starttime_ = now_millisec();
    return 0;
}

int RtcDtls::GenPrivateCert() {
    const uint8_t *aor = (uint8_t*)"cppstreamer.org";

    dtls_cert_ = X509_new();
    if (!dtls_cert_) {
        LogErrorf(logger_, "X509_new error");
        return -1;
    }

    /* Generate a self-signed certificate. */
    X509_NAME* subject = X509_NAME_new();
    if (!subject) {
        LogErrorf(logger_, "X509_NAME_new error");
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
        return -1;
    }

    int serial = (int)ByteCrypto::GetRandomUint(0, 65535);
    if (ASN1_INTEGER_set(X509_get_serialNumber(dtls_cert_), serial) != 1) {
        LogErrorf(logger_, "X509_get_serialNumber error");
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
        return -1;
    }

    if (X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, 
        aor, strlen((const char*)aor), -1, 0) != 1) {
        LogErrorf(logger_, "X509_NAME_add_entry_by_txt error");
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
        return -1;
    }

    if (X509_set_issuer_name(dtls_cert_, subject) != 1) {
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
        LogErrorf(logger_, "X509_set_issuer_name error");
        return -1;
    }
    if (X509_set_subject_name(dtls_cert_, subject) != 1) {
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
        LogErrorf(logger_, "X509_set_subjectname error");
        return -1;
    }

    int expire_day = 365;
    if (!X509_gmtime_adj(X509_get_notBefore(dtls_cert_), 0)) {
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
        LogErrorf(logger_, "X509_get_notBefore error");
        return -1;
    }
    if (!X509_gmtime_adj(X509_get_notAfter(dtls_cert_),
        60*60*24*expire_day)) {
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
        LogErrorf(logger_, "X509_get_notAfter error");
        return -1;
    }

    if (X509_set_version(dtls_cert_, 2) != 1) {
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
        LogErrorf(logger_, "X509_set_version error");
        return -1;
    }

    if (X509_set_pubkey(dtls_cert_, dtls_pkey_) != 1) {
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
        LogErrorf(logger_, "X509_set_pubkey error");
        return -1;
    }

    if (!X509_sign(dtls_cert_, dtls_pkey_, EVP_sha1())) {
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
        LogErrorf(logger_, "X509_sign error");
        return -1;
    }

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int n;
    /* Generate the fingerpint of certficate. */
    if (X509_digest(dtls_cert_, EVP_sha256(), md, &n) != 1) {
        X509_free(dtls_cert_);
        dtls_cert_ = nullptr;
        LogErrorf(logger_, "X509_digest error");
        return -1;
    }
    int len = 0;
    char fingerprint[8192];
    for (unsigned int i = 0; i < n; i++) {
        len += snprintf(fingerprint + len, 
                sizeof(fingerprint) - len, "%02X", md[i]);
        if (i < n - 1) {
            len += snprintf(fingerprint + len, sizeof(fingerprint) - len, ":");
        }
    }

    fingerprint_ = fingerprint;

    X509_NAME_free(subject);

    return 0;
}

int RtcDtls::InitContext() {
    int ret = 0;
    BIO *bio_out = NULL;

#if OPENSSL_VERSION_NUMBER < 0x10002000L /* OpenSSL v1.0.2 */
    ctx_ = SSL_CTX_new(DTLSv1_method());
#else
    ctx_ = SSL_CTX_new(DTLS_method());
#endif
    if (!ctx_) {
        LogErrorf(logger_, "SSL_CTX_new error");
        return -1;
    }

#if OPENSSL_VERSION_NUMBER >= 0x10002000L /* OpenSSL 1.0.2 */
    /* For ECDSA, we could set the curves list. */
    if (SSL_CTX_set1_curves_list(ctx_, "P-521:P-384:P-256") != 1) {
        LogErrorf(logger_, "DTLS: SSL_CTX_set1_curves_list failed");
        return -1;
    }
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L // v1.1.x
#if OPENSSL_VERSION_NUMBER < 0x10002000L // v1.0.2
    SSL_CTX_set_tmp_ecdh(ctx_, dtls_eckey_);
#else
    SSL_CTX_set_ecdh_auto(ctx_, 1);
#endif
#endif

    /**
     * We use "ALL", while you can use "DEFAULT" means "ALL:!EXPORT:!LOW:!aNULL:!eNULL:!SSLv2"
     *      Cipher Suite: ECDHE-ECDSA-AES128-CBC-SHA (0xc009)
     *      Cipher Suite: ECDHE-RSA-AES128-CBC-SHA (0xc013)
     *      Cipher Suite: ECDHE-ECDSA-AES256-CBC-SHA (0xc00a)
     *      Cipher Suite: ECDHE-RSA-AES256-CBC-SHA (0xc014)
     */
    if (SSL_CTX_set_cipher_list(ctx_, "ALL") != 1) {
        LogErrorf(logger_, "DTLS: SSL_CTX_set_cipher_list failed");
        return -1;
    }
    /* Setup the certificate. */
    if (SSL_CTX_use_certificate(ctx_, dtls_cert_) != 1) {
        LogErrorf(logger_, "DTLS: SSL_CTX_use_certificate failed");
        return -1;
    }
    if (SSL_CTX_use_PrivateKey(ctx_, dtls_pkey_) != 1) {
        LogErrorf(logger_, "DTLS: SSL_CTX_use_PrivateKey failed");
        return -1;
    }

    /* Server will send Certificate Request. */
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, on_ssl_certificate_verify);
    /* The depth count is "level 0:peer certificate", "level 1: CA certificate",
     * "level 2: higher level CA certificate", and so on. */
    SSL_CTX_set_verify_depth(ctx_, 4);
    /* Whether we should read as many input bytes as possible (for non-blocking reads) or not. */
    SSL_CTX_set_read_ahead(ctx_, 1);
    /* Only support SRTP_AES128_CM_SHA1_80, please read ssl/d1_srtp.c */
    if (SSL_CTX_set_tlsext_use_srtp(ctx_, "SRTP_AES128_CM_SHA1_80")) {
        LogErrorf(logger_, "DTLS: SSL_CTX_set_tlsext_use_srtp failed");
        return -1;
    }

    /* The dtls should not be created unless the dtls_ctx has been initialized. */
    dtls_ = SSL_new(ctx_);
    if (!dtls_) {
        return -1;
    }

    /* Setup the callback for logging. */
    SSL_set_ex_data(dtls_, 0, this);
    SSL_set_info_callback(dtls_, ssl_info_callback);

    /**
     * We have set the MTU to fragment the DTLS packet. It is important to note that the
     * packet is split to ensure that each handshake packet is smaller than the MTU.
     */
    SSL_set_options(dtls_, SSL_OP_NO_QUERY_MTU);
    SSL_set_mtu(dtls_, DTLS_MTU);
#if OPENSSL_VERSION_NUMBER >= 0x100010b0L /* OpenSSL 1.0.1k */
    DTLS_set_link_mtu(dtls_, DTLS_MTU);
#endif

    bio_in_ = BIO_new(BIO_s_mem());
    if (!bio_in_) {
        return -1;
    }

    bio_out = BIO_new(BIO_s_mem());
    if (!bio_out) {
        return -1;
    }

    /**
     * Please be aware that it is necessary to use a callback to obtain the packet to be written out. It is
     * imperative that BIO_get_mem_data is not used to retrieve the packet, as it returns all the bytes that
     * need to be sent out.
     * For example, if MTU is set to 1200, and we got two DTLS packets to sendout:
     *      ServerHello, 95bytes.
     *      Certificate, 1105+143=1248bytes.
     * If use BIO_get_mem_data, it will return 95+1248=1343bytes, which is larger than MTU 1200.
     * If use callback, it will return two UDP packets:
     *      ServerHello+Certificate(Frament) = 95+1105=1200bytes.
     *      Certificate(Fragment) = 143bytes.
     * Note that there should be more packets in real world, like ServerKeyExchange, CertificateRequest,
     * and ServerHelloDone. Here we just use two packets for example.
     */
#if OPENSSL_VERSION_NUMBER < 0x30000000L // v3.0.x
    BIO_set_callback(bio_out, openssl_dtls_bio_out_callback);
#else
    BIO_set_callback_ex(bio_out, openssl_dtls_bio_out_callback_ex);
#endif
    BIO_set_callback_arg(bio_out, (char*)this);
    SSL_set_bio(dtls_, bio_in_, bio_out);

    return ret;
}

int RtcDtls::OnState(enum DTLSState state, const char* type, const char* desc) {
    int ret = 0;

    if (state == DTLS_STATE_CLOSED) {
        int64_t now_ms = now_millisec();
        dtls_closed_ = true;
        LogInfof(logger_, "WHIP: DTLS session closed, type=%s, desc=%s, elapsed=%dms",
            type ? type : "", desc ? desc : "", RTC_ELAPSED(rtc_starttime_, now_ms));
        return ret;
    }

    if (state == DTLS_STATE_FAILED) {
        state_ = DTLS_STATE_FAILED;
        LogErrorf(logger_, "WHIP: DTLS session failed, type=%s, desc=%s",
            type ? type : "", desc ? desc : "");
        return ret;
    }

    if (state == DTLS_STATE_FINISHED && state_ < DTLS_STATE_FINISHED) {
        int64_t now_ms = now_millisec();

        state_ = DTLS_STATE_FINISHED;
        rtc_dtls_time_ = now_ms;
        LogInfof(logger_, "WHIP: DTLS handshake, done=%d, exported=%d, arq=%d, srtp_material=%luB, cost=%dms, elapsed=%dms",
            dtls_done_for_us_, dtls_srtp_key_exported_, dtls_arq_packets_, sizeof(dtls_srtp_materials_),
            RTC_ELAPSED(dtls_handshake_starttime_, dtls_handshake_endtime_),
            RTC_ELAPSED(rtc_starttime_, now_ms));
        return ret;
    }

    return ret;
}

void RtcDtls::OnDtlsData(uint8_t* buf, int size) {
    int ret = 0;
    int res_ct = 0;
    int res_ht = 0;
    int r0 = 0;
    int r1 = 0;
    bool do_callback = false;
    const char* dst = "EXTRACTOR-dtls_srtp";

    /* Got DTLS response successfully. */
    DtlsStateTrace(this, buf, size, 1);
    if ((r0 = BIO_write(bio_in_, buf, size)) <= 0) {
        res_ct = size > 0 ? buf[0]: 0;
        res_ht = size > 13 ? buf[13] : 0;
        LogErrorf(logger_, "DTLS: Feed response failed, content=%d, handshake=%d, size=%d, r0=%d",
            res_ct, res_ht, size, r0);
        return;
    }

    /**
     * If there is data available in bio_in, use SSL_read to allow SSL to process it.
     * We limit the MTU to 1200 for DTLS handshake, which ensures that the buffer is large enough for reading.
     */
    r0 = SSL_read(dtls_, buf, sizeof(buf));
    r1 = openssl_ssl_get_error(this, r0);
    if (r0 <= 0) {
        if (r1 != SSL_ERROR_WANT_READ && r1 != SSL_ERROR_WANT_WRITE && r1 != SSL_ERROR_ZERO_RETURN) {
            LogErrorf(logger_, "DTLS: Read failed, r0=%d, r1=%d %s", r0, r1, error_message);
            return;
        }
    } else {
        LogInfof(logger_, "DTLS: Read %d bytes, r0=%d, r1=%d", r0, r0, r1);
    }

    /* Check whether the DTLS is completed. */
    if (SSL_is_init_finished(dtls_) != 1) {
        LogInfof(logger_, "SSL_is_init_finished...");
        return;
    }

    do_callback = dtls_done_for_us_;
    dtls_done_for_us_ = true;
    dtls_handshake_endtime_ = now_millisec();

    /* Export SRTP master key after DTLS done */
    if (!dtls_srtp_key_exported_) {
        ret = SSL_export_keying_material(dtls_, dtls_srtp_materials_, sizeof(dtls_srtp_materials_),
            dst, strlen(dst), NULL, 0, 0);
        r1 = openssl_ssl_get_error(this, r0);
        if (!ret) {
            LogErrorf(logger_, "DTLS: SSL export key ret=%d, r1=%d %s", ret, r1, error_message);
            return;
        }

        dtls_srtp_key_exported_ = 1;
    }

    if (do_callback && (ret = OnState(DTLS_STATE_FINISHED, NULL, NULL)) < 0) {
        return;
    }

    SetupSRtp(CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_80);

    return;
}

int RtcDtls::OnWrite(uint8_t* data, int size) {

    if (udp_client_ == nullptr) {
        LogErrorf(logger_, "udp client is not ready");
        return -1;
    }

    LogInfof(logger_, "dtls write data len:%d, remote address:%s", 
            size, remote_address_.to_string().c_str());
    udp_client_->Write((char*)data, size, remote_address_);

    return 0;
}

int RtcDtls::DtlsStart()
{
    int ret = 0, r0, r1;
    char detail_error[256];

    LogInfof(logger_, "dtls start...");
    dtls_handshake_starttime_ = now_millisec();

    SSL_set_accept_state(dtls_);
    LogInfof(logger_, "dtls work in %s mode", (SSL_is_server(dtls_) == 1) ? "server" : "client");

    r0 = SSL_do_handshake(dtls_);
    r1 = SSL_get_error(dtls_, r0);
    if (r0 < 0 && r1 == SSL_ERROR_SSL) {
        ERR_error_string_n(ERR_get_error(), detail_error, sizeof(detail_error));
    }
    ERR_clear_error();
    // Fatal SSL error, for example, no available suite when peer is DTLS 1.0 while we are DTLS 1.2.
    if (r0 < 0 && (r1 != SSL_ERROR_NONE && r1 != SSL_ERROR_WANT_READ && r1 != SSL_ERROR_WANT_WRITE)) {
        LogErrorf(logger_, "Failed to drive SSL context, r0=%d, r1=%d %s", r0, r1, detail_error);
        return -1;
    }

    return ret;
}

int RtcDtls::SetupSRtp(CRYPTO_SUITE_ENUM srtp_suite) {
    size_t srtp_keylength{ 0 };
    size_t srtp_saltlength{ 0 };
    size_t srtp_masterlength{ 0 };

    switch (srtp_suite)
    {
        case CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_80:
        case CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_32:
        {
            srtp_keylength    = SRTP_MASTER_KEY_LENGTH;
            srtp_saltlength   = SRTP_MASTER_SALT_LENGTH;
            srtp_masterlength = SRTP_MASTER_LENGTH;
            break;
        }

        case CRYPTO_SUITE_AEAD_AES_256_GCM:
        {
            srtp_keylength    = SRTP_AESGCM_256_MASTER_KEY_LENGTH;
            srtp_saltlength   = SRTP_AESGCM_256_MASTER_SALT_LENGTH;
            srtp_masterlength = SRTP_AESGCM_256_MASTER_LENGTH;
            break;
        }

        case CRYPTO_SUITE_AEAD_AES_128_GCM:
        {
            srtp_keylength    = SRTP_AESGCM_128_MASTER_KEY_LENGTH;
            srtp_saltlength   = SRTP_AESGCM_128_MASTER_SALT_LENGTH;
            srtp_masterlength = SRTP_AESGCM_128_MASTER_LENGTH;

            break;
        }
        default:
        {
            LogErrorf(logger_, "unknown srtp crypto suite");
            return -1;
        }
    }

    uint8_t* srtp_material = new uint8_t[srtp_masterlength * 2];
    uint8_t* srtp_local_key  = nullptr;
    uint8_t* srtp_local_salt = nullptr;
    uint8_t* srtp_remote_key = nullptr;
    uint8_t* srtp_remote_salt = nullptr;
    
    uint8_t* srtp_local_masterkey  = new uint8_t[srtp_masterlength];
    uint8_t* srtp_remote_masterkey = new uint8_t[srtp_masterlength];

    int ret = SSL_export_keying_material(dtls_, srtp_material, srtp_masterlength * 2,
                            "EXTRACTOR-dtls_srtp", strlen("EXTRACTOR-dtls_srtp"), nullptr, 0, 0);

    if (ret != 1) {
        LogErrorf(logger_, "SSL_export_keying_material error:%d", ret);
        delete[] srtp_material;
        delete[] srtp_local_masterkey;
        delete[] srtp_remote_masterkey;
        return -1;
    }


    srtp_remote_key  = srtp_material;
    srtp_local_key   = srtp_remote_key + srtp_keylength;
    srtp_remote_salt = srtp_local_key + srtp_keylength;
    srtp_local_salt  = srtp_remote_salt + srtp_saltlength;

        
    memcpy(srtp_local_masterkey, srtp_local_key, srtp_keylength);
    memcpy(srtp_local_masterkey + srtp_keylength, srtp_local_salt, srtp_saltlength);

    // Create the SRTP remote master key.
    memcpy(srtp_remote_masterkey, srtp_remote_key, srtp_keylength);
    memcpy(srtp_remote_masterkey + srtp_keylength, srtp_remote_salt, srtp_saltlength);


    LogInfof(logger_, "srtp init connected....");

    //set srtp parameters
    pc_->OnDtlsConnected(srtp_suite,
                srtp_local_masterkey, srtp_masterlength,
                srtp_remote_masterkey, srtp_masterlength);

    delete[] srtp_material;
    delete[] srtp_local_masterkey;
    delete[] srtp_remote_masterkey;

    return 0;
}

bool RtcDtls::IsDtls(const uint8_t* data, size_t len) {
    //https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
    return ((len >= 13) && (data[0] > 19 && data[0] < 64));
}


}

