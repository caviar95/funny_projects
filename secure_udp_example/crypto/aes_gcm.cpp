
#include "aes_gcm.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <cstring>

bool aes_gcm_encrypt(const std::vector<uint8_t>& key,
                     const std::string& plaintext,
                     std::vector<uint8_t>& nonce,
                     std::vector<uint8_t>& ciphertext,
                     std::vector<uint8_t>& tag) {
    nonce.resize(12);
    RAND_bytes(nonce.data(), 12);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    int len;
    ciphertext.resize(plaintext.size());
    tag.resize(16);

    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) return false;
    EVP_EncryptInit_ex(ctx, NULL, NULL, key.data(), nonce.data());

    if (!EVP_EncryptUpdate(ctx, ciphertext.data(), &len, (const uint8_t*)plaintext.data(), plaintext.size())) return false;
    int ciphertext_len = len;

    if (!EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len)) return false;
    ciphertext_len += len;

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data())) return false;

    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(ciphertext_len);
    return true;
}

bool aes_gcm_decrypt(const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& nonce,
                     const std::vector<uint8_t>& ciphertext,
                     const std::vector<uint8_t>& tag,
                     std::string& plaintext) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    int len;
    std::vector<uint8_t> out(ciphertext.size());

    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) return false;
    EVP_DecryptInit_ex(ctx, NULL, NULL, key.data(), nonce.data());

    if (!EVP_DecryptUpdate(ctx, out.data(), &len, ciphertext.data(), ciphertext.size())) return false;
    int plaintext_len = len;

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)tag.data());

    int ret = EVP_DecryptFinal_ex(ctx, out.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);
    if (ret <= 0) return false;

    plaintext_len += len;
    plaintext.assign((char*)out.data(), plaintext_len);
    return true;
}
