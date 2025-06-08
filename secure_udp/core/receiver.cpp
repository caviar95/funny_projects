#include "receiver.h"
#include "../config/config.h"
#include "../crypto/aes_gcm.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <chrono>

SecureUdpReceiver::SecureUdpReceiver(int localPort)
    : running_(false) {
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        perror("socket");
        throw std::runtime_error("Failed to create socket");
    }

    struct sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(localPort);

    if (bind(sockfd_, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        perror("bind");
        close(sockfd_);
        throw std::runtime_error("Failed to bind socket");
    }
}

SecureUdpReceiver::~SecureUdpReceiver() {
    stop();
}

void SecureUdpReceiver::start(std::function<void(const std::string&)> onMessage) {
    callback_ = std::move(onMessage);
    running_ = true;
    receiveThread_ = std::thread(&SecureUdpReceiver::receiveThreadFunc, this);
}

void SecureUdpReceiver::stop() {
    if (running_) {
        running_ = false;
        if (receiveThread_.joinable()) receiveThread_.join();
        close(sockfd_);
    }
}

void SecureUdpReceiver::receiveThreadFunc() {
    std::vector<uint8_t> buffer(1500);
    while (running_) {
        ssize_t len = recvfrom(sockfd_, buffer.data(), buffer.size(), 0, nullptr, nullptr);
        if (len <= 0) continue;

        if (len < 4 + 8 + 12 + 16) continue; // SEQ(4) + TIMESTAMP(8) + NONCE(12) + TAG(16)

        size_t offset = 0;
        uint32_t seq = 0;
        for (int i = 0; i < 4; i++) seq |= (buffer[offset++] << (i * 8));

        uint64_t timestamp = 0;
        for (int i = 0; i < 8; i++) timestamp |= (uint64_t(buffer[offset++]) << (i * 8));

        std::vector<uint8_t> nonce(buffer.begin() + offset, buffer.begin() + offset + 12);
        offset += 12;

        size_t cipherLen = len - offset - 16;
        std::vector<uint8_t> cipher(buffer.begin() + offset, buffer.begin() + offset + cipherLen);
        std::vector<uint8_t> tag(buffer.begin() + offset + cipherLen, buffer.begin() + offset + cipherLen + 16);

        std::string plaintext;
        bool ok = aes_gcm_decrypt(std::vector<uint8_t>(SHARED_KEY.begin(), SHARED_KEY.end()),
                                  nonce, cipher, tag, plaintext);
        if (!ok) {
            std::cerr << "Decryption failed for packet seq=" << seq << "\n";
            continue;
        }

        if (callback_) callback_(plaintext);
    }
}

