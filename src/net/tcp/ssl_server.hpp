#ifndef SSL_SERVER_HPP
#define SSL_SERVER_HPP
#include "logger.hpp"
#include "ssl_pub.hpp"

#include <memory>
#include <string>
#include <stdint.h>
#include <iostream>
#include <stdio.h>
#include <queue>
#include <sstream>
#include <openssl/ssl.h>
#include <assert.h>

namespace cpp_streamer
{

class SslServer
{
public:
    SslServer(const std::string& key_file,
            const std::string& cert_file,
            SslCallbackI* cb,
            Logger* logger = nullptr):key_file_(key_file)
                          , cert_file_(cert_file)
                          , cb_(cb)
                          , logger_(logger)
    {
        plaintext_data_ = new uint8_t[plaintext_data_len_];
        LogInfof(logger_, "SslServer construct ...");
    }
    ~SslServer() {
        if (ssl_) {
            SSL_free(ssl_);
            ssl_ = NULL;
        }
    
        if (ssl_ctx_) {
            SSL_CTX_free(ssl_ctx_);
            ssl_ctx_ = NULL;
        }
        if (plaintext_data_) {
            delete[] plaintext_data_;
            plaintext_data_ = nullptr;
        }
    }

public:
    TLS_SERVER_STATE GetState() {
        return tls_state_;
    }

    int Handshake(char* buf, size_t nn) {
        int ret = 0;

        if (tls_state_ >= TLS_SERVER_KEY_EXCHANGE_DONE) {
            return 0;
        }

        ret = SslInit();
        if (ret != 0) {
            return ret;
        }
        switch (tls_state_)
        {
            case TLS_SSL_SERVER_INIT_DONE:
            {
                ret = HandleTlsHello(buf, nn);
                if (ret != 0) {
                    return ret;
                }
                tls_state_ = TLS_SSL_SERVER_HELLO_DONE;
                break;
            }
            case TLS_SSL_SERVER_HELLO_DONE:
            {
                ret = HandleKeyExchange(buf, nn);
                if (ret != 0) {
                    return ret;
                }
                break;
            }
            default:
                break;
        }
        return 0;
    }

    int HandleSslDataRecv(uint8_t* data, size_t len) {
        if (tls_state_ < TLS_SERVER_KEY_EXCHANGE_DONE) {
            return 0;
        }

        if (tls_state_ == TLS_SERVER_KEY_EXCHANGE_DONE) {
            tls_state_ = TLS_SERVER_DATA_RECV_STATE;
            return 0;
        }

        int r0 = BIO_write(bio_in_, data, len);
        if (r0 <= 0) {
            LogErrorf(logger_, "BIO_write error:%d", r0);
            return -1;
        }
        //while(true) {
            r0 = SSL_read(ssl_, plaintext_data_, plaintext_data_len_);
            int r1 = SSL_get_error(ssl_, r0);
            int r2 = BIO_ctrl_pending(bio_in_);
            int r3 = SSL_is_init_finished(ssl_);

            // OK, got data.
            if (r0 > 0) {
                cb_->PlaintextDataRecv((char*)plaintext_data_, r0);
            } else {
                LogDebugf(logger_, "SSL_read error, r0:%d, r1:%d, r2:%d, r3:%d",
                        r0, r1, r2, r3);
                //break;
            }
        //}
        return 0;
    }

    int SslWrite(uint8_t* plain_text_data, size_t len) {
        int writen_len = 0;

        for (char* p = (char*)plain_text_data; p < (char*)plain_text_data + len;) {
            int left = (int)len - (p - (char*)plain_text_data);
            int r0 = SSL_write(ssl_, (const void*)p, left);
            int r1 = SSL_get_error(ssl_, r0);
            if (r0 <= 0) {
                LogErrorf(logger_, "ssl write data=%p, size=%d, r0=%d, r1=%d",
                        p, left, r0, r1);
                return -1;
            }
    
            // Move p to the next writing position.
            p += r0;
            writen_len += (ssize_t)r0;
    
            uint8_t* data = NULL;
            int size = BIO_get_mem_data(bio_out_, &data);
            cb_->PlaintextDataSend((char*)data, size);

            if ((r0 = BIO_reset(bio_out_)) != 1) {
                LogErrorf(logger_, "BIO_reset r0=%d", r0);
                return -1;
            }
        }

        return writen_len;
    }

private:
    int SslInit() {
        if (tls_state_ >= TLS_SSL_SERVER_INIT_DONE) {
            return 0;
        }

    #if (OPENSSL_VERSION_NUMBER < 0x10002000L) // v1.0.2
        ssl_ctx_ = SSL_CTX_new(TLS_method());
    #else
        ssl_ctx_ = SSL_CTX_new(TLSv1_2_method());
    #endif
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, NULL);
        assert(SSL_CTX_set_cipher_list(ssl_ctx_, "ALL") == 1);

        if ((ssl_ = SSL_new(ssl_ctx_)) == NULL) {
            LogErrorf(logger_, "ssl new error");
            return -1;
        }
    
        if ((bio_in_ = BIO_new(BIO_s_mem())) == NULL) {
            LogErrorf(logger_, "BIO_new in error");
            return -1;
        }
    
        if ((bio_out_ = BIO_new(BIO_s_mem())) == NULL) {
            LogErrorf(logger_, "BIO_new in error");
            BIO_free(bio_in_);
            return -1;
        }
    
        SSL_set_bio(ssl_, bio_in_, bio_out_);
    
        // SSL setup active, as server role.
        SSL_set_accept_state(ssl_);
        SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);
    
        int r0;
    
        // Setup the key and cert file for server.
        if ((r0 = SSL_use_certificate_file(ssl_, cert_file_.c_str(), SSL_FILETYPE_PEM)) != 1) {
            LogErrorf(logger_, "SSL_use_certificate_file error, cert file:%s, return %d", cert_file_.c_str(), r0);
            return -1;
        }
    
        if ((r0 = SSL_use_RSAPrivateKey_file(ssl_, key_file_.c_str(), SSL_FILETYPE_PEM)) != 1) {
            LogErrorf(logger_, "SSL_use_RSAPrivateKey_file error, key file:%s, return %d", key_file_.c_str(), r0);
            return -1;
        }
    
        if ((r0 = SSL_check_private_key(ssl_)) != 1) {
            LogErrorf(logger_, "SSL_check_private_key error, return %d", r0);
            return -1;
        }
        tls_state_ = TLS_SSL_SERVER_INIT_DONE;

        LogInfof(logger_, "ssl init done ....");
        
        return 0;
    }


    int HandleTlsHello(char* buf, size_t nn) {
        int r0 = 0;
        int r1 = 0;
        size_t size = 0;
        uint8_t* data = nullptr;

        if ((r0 = BIO_write(bio_in_, buf, nn)) <= 0) {
            LogErrorf(logger_, "client hello BIO_write error:%d", r0);
            return -1;
        }
    
        r0 = SSL_do_handshake(ssl_);
        r1 = SSL_get_error(ssl_, r0);
        if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
            LogErrorf(logger_, "client hello BIO_write error r0:%d, r1:%d", r0, r1);
            return -1;
        }
    
        if ((size = BIO_get_mem_data(bio_out_, &data)) > 0) {
            if ((r0 = BIO_reset(bio_in_)) != 1) {
                LogInfof(logger_, "BIO_reset error:%d", r0);
                return -1;
            }
        }

        size = BIO_get_mem_data(bio_out_, &data);
        if (!data || size <= 0) {
            LogErrorf(logger_, "BIO_get_mem_data error");
            return -1;
        }

        cb_->PlaintextDataSend((char*)data, size);

        if ((r0 = BIO_reset(bio_out_)) != 1) {
            LogErrorf(logger_, "BIO_reset error:%d", r0);
            return -1;
        }

        return 0;
    }

    int HandleKeyExchange(char* buf, size_t nn) {
        int r0 = 0;
        int r1 = 0;
        size_t size = 0;
        char* data  = nullptr;

        if ((r0 = BIO_write(bio_in_, buf, nn)) <= 0) {
            LogErrorf(logger_, "BIO_write error:%d", r0);
            return -1;
        }

        r0 = SSL_do_handshake(ssl_);
        r1 = SSL_get_error(ssl_, r0);
        if (r0 == 1 && r1 == SSL_ERROR_NONE) {
            size = BIO_get_mem_data(bio_out_, &data);
            if (!data || size <= 0) {
                LogErrorf(logger_, "BIO_get_mem_data error");
                return -1;
            }
        } else {
            if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
                LogErrorf(logger_, "SSL_do_handshake error, r0:%d, r1:%d", r0, r1);
                return -1;
            }
    
            if ((size = BIO_get_mem_data(bio_out_, &data)) > 0) {
                if ((r0 = BIO_reset(bio_in_)) != 1) {
                    LogErrorf(logger_, "BIO_reset error");
                    return -1;
                }
            }
        }

        // Send New Session Ticket, Change Cipher Spec, Encrypted Handshake Message
        cb_->PlaintextDataSend((char*)data, size);

        if ((r0 = BIO_reset(bio_out_)) != 1) {
            LogErrorf(logger_, "BIO_reset error");
            return -1;
        }
        tls_state_ = TLS_SERVER_KEY_EXCHANGE_DONE;
        return 0;
    }

private:
    std::string key_file_;
    std::string cert_file_;
    SslCallbackI* cb_ = nullptr;
    Logger* logger_ = nullptr;

private:
    SSL_CTX* ssl_ctx_ = nullptr;
    SSL* ssl_         = nullptr;
    BIO* bio_in_      = nullptr;
    BIO* bio_out_     = nullptr;
    uint8_t* plaintext_data_ = nullptr;
    size_t plaintext_data_len_ = SSL_DEF_RECV_BUFFER_SIZE;

private:
    TLS_SERVER_STATE tls_state_ = TLS_SSL_SERVER_ZERO;
};

}
#endif //SSL_SERVER_HPP
