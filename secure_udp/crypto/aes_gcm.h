#pragma once

#include <vector>
#include <string>
#include <cstdint>

bool aes_gcm_encrypt(const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& nonce,
                     const std::string& plaintext,
                     std::vector<uint8_t>& ciphertext,
                     std::vector<uint8_t>& tag);

bool aes_gcm_decrypt(const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& nonce,
                     const std::vector<uint8_t>& ciphertext,
                     const std::vector<uint8_t>& tag,
                     std::string& plaintext);

