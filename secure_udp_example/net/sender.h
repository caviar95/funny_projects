
#pragma once
#include <string>
#include <netinet/in.h>

class SecureUdpSender {
public:
    SecureUdpSender(const std::string& destIp, int destPort);
    void send(const std::string& message);

private:
    int sockfd_;
    struct sockaddr_in destAddr_;
    uint32_t sequence_;
};
