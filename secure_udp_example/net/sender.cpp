
#include "sender.h"
#include "../config/config.h"
#include "../crypto/aes_gcm.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <chrono>
#include <iostream>

SecureUdpSender::SecureUdpSender(const std::string& destIp, int destPort)
    : sequence_(0) {
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&destAddr_, 0, sizeof(destAddr_));
    destAddr_.sin_family = AF_INET;
    destAddr_.sin_port = htons(destPort);
    inet_pton(AF_INET, destIp.c_str(), &destAddr_.sin_addr);
}

void SecureUdpSender::send(const std::string& message) {
    std::vector<uint8_t> nonce, ciphertext, tag;
    if (!aes_gcm_encrypt(std::vector<uint8_t>(SHARED_KEY.begin(), SHARED_KEY.end()), message, nonce, ciphertext, tag)) {
        std::cerr << "Encryption failed\n";
        return;
    }

    std::vector<uint8_t> packet;
    uint32_t seq = sequence_++;
    for (int i = 0; i < 4; ++i) packet.push_back((seq >> (i * 8)) & 0xFF);

    uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count();
    for (int i = 0; i < 8; ++i) packet.push_back((ts >> (i * 8)) & 0xFF);

    packet.insert(packet.end(), nonce.begin(), nonce.end());
    packet.insert(packet.end(), ciphertext.begin(), ciphertext.end());
    packet.insert(packet.end(), tag.begin(), tag.end());

    sendto(sockfd_, packet.data(), packet.size(), 0, (struct sockaddr*)&destAddr_, sizeof(destAddr_));
}
