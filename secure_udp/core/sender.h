#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <netinet/in.h>

class SecureUdpSender {
public:
    SecureUdpSender(const std::string& remoteIp, int remotePort);
    ~SecureUdpSender();

    bool send(const std::string& data);
    void stop();

private:
    void sendThreadFunc();

    int sockfd_;
    struct sockaddr_in remoteAddr_;
    std::atomic<uint32_t> seq_;

    std::unordered_map<uint32_t, std::string> unackedPackets_;
    std::mutex mu_;
    std::condition_variable cv_;

    std::thread sendThread_;
    std::atomic<bool> running_;
};

