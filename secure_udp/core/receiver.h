#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <netinet/in.h>
#include <functional>

class SecureUdpReceiver {
public:
    SecureUdpReceiver(int localPort);
    ~SecureUdpReceiver();

    void start(std::function<void(const std::string&)> onMessage);
    void stop();

private:
    void receiveThreadFunc();

    int sockfd_;
    std::atomic<bool> running_;
    std::thread receiveThread_;
    std::function<void(const std::string&)> callback_;
};

