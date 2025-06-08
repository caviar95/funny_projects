#include "aes_gcm.h"

#include <sodium.h>

bool aes_gcm_encrypt(const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& nonce,
                     const std::string& plaintext,
                     std::vector<uint8_t>& ciphertext,
                     std::vector<uint8_t>& tag) {
    if (key.size() != crypto_aead_aes256gcm_KEYBYTES) {
        return false;
    }

    if (nonce.size() != crypto_aead_aes256gcm_KEYBYTES) {
        return false;
    }

    ciphertext.resize(plaintext.size() + crypto_aead_aes256gcm_KEYBYTES);

    unsigned long long clen{};
    if (crypto_aead_aes256gcm_encrypt(ciphertext.data(), &clen,
                static_cast<const unsigned char*>(plaintext.data()), plaintext.size(),
                nullptr, 0, nullptr,
                nonce.data(), key.data()) != 0) {
        return false;
    }

    ciphertext.resize(clen);

    tag.assign(ciphertext.end() - crypto_aead_aes256gcm_ABYTES, ciphertext.end());
    ciphertext.resize(clen - crypto_aead_aes256gcm_ABYTES);

    return true;
}

bool aes_gcm_decrypt(const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& nonce,
                     const std::vector<uint8_t>& ciphertext,
                     const std::vector<uint8_t>& tag,
                     std::string& plaintext) {
    if (key.size() != crypto_aead_aes256gcm_KEYBYTES) {
        return false;
    }

    if (nonce.size() != crypto_aead_aes256gcm_KEYBYTES) {
        return false;
    }

    std::vector<uint8_t> combined(ciphertext);
    combined.insert(combined.end(), tag.begin(), tag.end());

    std::vector<uint8_t> decrypted(ciphertext.size());

    unsigned long long declen;

    if (crypto_aead_aes256gcm_decrypt(decrypted.data(), &declen, 
                                      nullptr,
                                      combined.data(), combined.size(),
                                      nullptr, 0,
                                      nonce.data(), key.data()) != 0) {
        return false;
    }

    plaintext.assign(static_cast<char*>(decrypted.data(), declen));

    return true;
}

