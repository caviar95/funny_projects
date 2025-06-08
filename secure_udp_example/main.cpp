
#include "net/sender.h"
#include "net/receiver.h"
#include <iostream>
#include <thread>

int main() {
    SecureUdpReceiver receiver(9000);
    receiver.start([](const std::string& msg) {
        std::cout << "[Received] " << msg << std::endl;
    });

    SecureUdpSender sender("127.0.0.1", 9000);

    std::string input;
    while (true) {
        std::cout << "Enter message: ";
        std::getline(std::cin, input);
        if (input == "exit") break;
        sender.send(input);
    }

    receiver.stop();
    return 0;
}
