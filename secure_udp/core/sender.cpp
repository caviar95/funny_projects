#include "sender.h"
#include "../config/config.h"
#include "../crypto/aes_gcm.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <iostream>
#include <random>

static std::vector<uint8_t> generateNonce() {
    std::vector<uint8_t> nonce(12);
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint8_t> dis(0, 255);
    for(auto &b : nonce) b = dis(gen);
    return nonce;
}

SecureUdpSender::SecureUdpSender(const std::string& remoteIp, int remotePort)
    : seq_(0), running_(true)
{
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        perror("socket");
        throw std::runtime_error("Failed to create socket");
    }

    remoteAddr_.sin_family = AF_INET;
    remoteAddr_.sin_port = htons(remotePort);
    inet_pton(AF_INET, remoteIp.c_str(), &remoteAddr_.sin_addr);

    sendThread_ = std::thread(&SecureUdpSender::sendThreadFunc, this);
}

SecureUdpSender::~SecureUdpSender() {
    stop();
}

bool SecureUdpSender::send(const std::string& data) {
    uint32_t currentSeq = seq_.fetch_add(1);

    // 加密数据
    std::vector<uint8_t> nonce = generateNonce();
    std::vector<uint8_t> cipherText;
    std::vector<uint8_t> tag;
    bool ret = aes_gcm_encrypt(
        std::vector<uint8_t>(SHARED_KEY.begin(), SHARED_KEY.end()),
        nonce,
        data,
        cipherText,
        tag);

    if (!ret) {
        std::cerr << "Encryption failed\n";
        return false;
    }

    // 组包: [SEQ(4B)][TIMESTAMP(8B)][NONCE(12B)][CIPHERTEXT][TAG(16B)]
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    std::vector<uint8_t> packet;
    // 写SEQ（小端）
    for(int i=0; i<4; i++) packet.push_back((currentSeq >> (i*8)) & 0xff);
    // 写TIMESTAMP（小端）
    for(int i=0; i<8; i++) packet.push_back((timestamp >> (i*8)) & 0xff);
    // 写NONCE
    packet.insert(packet.end(), nonce.begin(), nonce.end());
    // 写CIPHERTEXT
    packet.insert(packet.end(), cipherText.begin(), cipherText.end());
    // 写TAG
    packet.insert(packet.end(), tag.begin(), tag.end());

    {
        std::lock_guard<std::mutex> lock(mu_);
        unackedPackets_[currentSeq] = std::string(packet.begin(), packet.end());
    }
    cv_.notify_one();
    return true;
}

void SecureUdpSender::sendThreadFunc() {
    while (running_) {
        std::unique_lock<std::mutex> lock(mu_);
        if (unackedPackets_.empty()) {
            cv_.wait(lock);
        }
        if (!running_) break;

        for (auto& p : unackedPackets_) {
            ssize_t sent = sendto(sockfd_, p.second.data(), p.second.size(), 0,
                    (struct sockaddr*)&remoteAddr_, sizeof(remoteAddr_));
            if (sent < 0) {
                perror("sendto");
            }
        }
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SecureUdpSender::stop() {
    if (running_) {
        running_ = false;
        cv_.notify_all();
        if (sendThread_.joinable()) sendThread_.join();
        close(sockfd_);
    }
}

