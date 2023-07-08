#ifndef SSL_CLIENT_H
#define SSL_CLIENT_H
#include "ssl_pub.hpp"
#include "logger.hpp"

#include <openssl/ssl.h>
#include <string>
#include <stdint.h>

namespace cpp_streamer
{
inline int on_verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
    // Always OK, we don't check the certificate of client,
    return 1;
}

class SslClient
{
public:
    SslClient(SslCallbackI* cb,
            Logger* logger = nullptr):cb_(cb)
                                      , logger_(logger)
    {
        plaintext_data_ = (uint8_t*)malloc(plaintext_data_len_);
        state_ = TLS_SSL_CLIENT_ZERO;
    }
    ~SslClient()
    {
        if (plaintext_data_) {
            delete[] plaintext_data_;
            plaintext_data_ = nullptr;
        }
    }

public:
    TLS_CLIENT_STATE GetState() {
        return state_;
    }

    int ClientHello() {
#if (OPENSSL_VERSION_NUMBER < 0x10002000L) // v1.0.2
        ssl_ctx_ = SSL_CTX_new(TLS_method());
#else
        ssl_ctx_ = SSL_CTX_new(TLSv1_2_method());
#endif
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, on_verify_callback);
        if (SSL_CTX_set_cipher_list(ssl_ctx_, "ALL") != 1) {
            LogErrorf(logger_, "SSL_CTX_set_cipher_list set all error");
            return -1;
        }

        if ((ssl_ = SSL_new(ssl_ctx_)) == NULL) {
            LogErrorf(logger_, "SSL_new error");
            return -1;
        }

        if ((bio_in_ = BIO_new(BIO_s_mem())) == NULL) {
            LogErrorf(logger_, "BIO_new bio_in error");
            return -1;
        }

        if ((bio_out_ = BIO_new(BIO_s_mem())) == NULL) {
            BIO_free(bio_in_);
            LogErrorf(logger_, "BIO_new bio_out error");
            return -1;
        }

        SSL_set_bio(ssl_, bio_in_, bio_out_);

        SSL_set_connect_state(ssl_);
        SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);

        // Send ClientHello.
        int r0 = SSL_do_handshake(ssl_); int r1 = SSL_get_error(ssl_, r0);
        if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
            LogErrorf(logger_, "handshake r0=%d, r1=%d", r0, r1);
            return -1;
        }

        uint8_t* data = NULL;
        int size = BIO_get_mem_data(bio_out_, &data);
        if (!data || size <= 0) {
            LogErrorf(logger_, "handshake data=%p, size=%d", data, size);
            return -1;
        }
        
        LogInfof(logger_, "send server hello......");
        cb_->PlaintextDataSend((char*)data, size);
 
        if ((r0 = BIO_reset(bio_out_)) != 1) {
            LogErrorf(logger_, "BIO_reset r0=%d", r0);
            return -1;
        }

        state_ = TLS_CLIENT_HELLO_DONE;
        LogDebugf(logger_, "ssl client: ClientHello done");
        return 0;
    }

    int RecvServerHello(char* buf, ssize_t nn) {
        int r0 = 0;
        int r1 = 0;
        bool ready = false;

        if ((r0 = BIO_write(bio_in_, buf, nn)) <= 0) {
            LogErrorf(logger_, "BIO_write r0=%d, data=%p, size=%d", r0, buf, nn);
            return -1;
        }

        if ((r0 = SSL_do_handshake(ssl_)) != -1 || (r1 = SSL_get_error(ssl_, r0)) != SSL_ERROR_WANT_READ) {
            LogErrorf(logger_, "handshake r0=%d, r1=%d", r0, r1);
            return -1;
        }

        char* data = nullptr;
        ssize_t size = 0;
        if ((size = BIO_get_mem_data(bio_out_, &data)) > 0) {
            if ((r0 = BIO_reset(bio_in_)) != 1) {
                LogErrorf(logger_, "BIO_reset r0=%d", r0);
                return -1;
            }
            ready = true;
        }
        if (!ready) {
            LogDebugf(logger_, "ssl hello need to read more");
            //need more data
            return 1;
        }

        // Send Client Key Exchange, Change Cipher Spec, Encrypted Handshake Message
        cb_->PlaintextDataSend(data, size);
        state_ = TLS_CLIENT_KEY_EXCHANGE;
        if ((r0 = BIO_reset(bio_out_)) != 1) {
            LogErrorf(logger_, "BIO_reset r0=%d", r0);
            return -1;
        }

        return 0;
    }


    int HandleSessionTicket(char* buf, ssize_t nn) {
        int r0 = 0;
        int r1 = 0;

        if ((r0 = BIO_write(bio_in_, buf, nn)) <= 0) {
            LogErrorf(logger_, "BIO_write r0=%d, data=%p, size=%d", r0, buf, nn);
            return -1;
        }

        r0 = SSL_do_handshake(ssl_); r1 = SSL_get_error(ssl_, r0);
        if (r0 == 1 && r1 == SSL_ERROR_NONE) {
            LogInfof(logger_, "Ssl client final done");
            state_ = TLS_CLIENT_READY;
            return 0;
        }

        if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
            LogErrorf(logger_, "handshake r0=%d, r1=%d", r0, r1);
            return -1;
        }
        //need more data
        return 1;
    }

    int HandleSslDataRecv(uint8_t* data, size_t len) {
        int r0, r1, r2, r3;
        r0 = SSL_read(ssl_, plaintext_data_, plaintext_data_len_);
        r1 = SSL_get_error(ssl_, r0);
        r2 = BIO_ctrl_pending(bio_in_);
        r3 = SSL_is_init_finished(ssl_);

        // OK, got data.
        if (r0 > 0) {
            cb_->PlaintextDataRecv((char*)plaintext_data_, r0);
        } else {
            LogDebugf(logger_, "SSL_read error, r0:%d, r1:%d, r2:%d, r3:%d",
                    r0, r1, r2, r3);
        }
 
        r0 = BIO_write(bio_in_, data, len);
        if (r0 <= 0) {
            LogErrorf(logger_, "BIO_write error:%d", r0);
            return -1;
        }
        do {
            r0 = SSL_read(ssl_, plaintext_data_, plaintext_data_len_);
            r1 = SSL_get_error(ssl_, r0);
            r2 = BIO_ctrl_pending(bio_in_);
            r3 = SSL_is_init_finished(ssl_);

            LogDebugf(logger_, "ssl read plain buflen:%d, r0:%d, r1:%d, r2:%d, r3:%d", plaintext_data_len_,
                    r0, r1, r2, r3);
            // OK, got data.
            if (r0 > 0) {
                cb_->PlaintextDataRecv((char*)plaintext_data_, r0);
            } else {
                if (r0 == -1 && r1 == SSL_ERROR_WANT_READ) {
                    return 0;
                }
                LogErrorf(logger_, "SSL_read error, r0:%d, r1:%d, r2:%d, r3:%d",
                        r0, r1, r2, r3);
                return -1;
            }
        } while(true);
   
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
            if (size > 0 && data != nullptr) {
                cb_->PlaintextDataSend((char*)data, size);
            }

            if ((r0 = BIO_reset(bio_out_)) != 1) {
                LogErrorf(logger_, "BIO_reset r0=%d", r0);
                return -1;
            }
        }

        return writen_len;
    }


private:
    SslCallbackI* cb_ = nullptr;
    SSL_CTX* ssl_ctx_ = nullptr;
    SSL* ssl_         = nullptr;
    BIO* bio_in_      = nullptr;
    BIO* bio_out_     = nullptr;
    TLS_CLIENT_STATE state_ = TLS_SSL_CLIENT_ZERO;

private:
    uint8_t* plaintext_data_    = nullptr;
    ssize_t plaintext_data_len_ = SSL_DEF_RECV_BUFFER_SIZE;
private:
    Logger* logger_ = nullptr;
};

}




#endif
