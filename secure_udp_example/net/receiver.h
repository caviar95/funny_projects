
#pragma once
#include <functional>
#include <string>
#include <thread>
#include <atomic>

class SecureUdpReceiver {
public:
    explicit SecureUdpReceiver(int localPort);
    ~SecureUdpReceiver();
    void start(std::function<void(const std::string&)> onMessage);
    void stop();

private:
    void receiveThreadFunc();
    int sockfd_;
    std::thread receiveThread_;
    std::atomic<bool> running_;
    std::function<void(const std::string&)> callback_;
};
