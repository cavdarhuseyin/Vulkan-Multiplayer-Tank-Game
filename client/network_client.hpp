#pragma once

#include "../shared/network_protocol.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace lve::net {

    class NetworkClient {
    public:
        NetworkClient();
        ~NetworkClient();

        NetworkClient(const NetworkClient&) = delete;
        NetworkClient& operator=(const NetworkClient&) = delete;

        bool connect(const std::string& host, int port, const std::string& nickname = "Player");
        void disconnect();

        bool isConnected() const { return connected; }
        std::uint32_t getLocalPlayerId() const { return localPlayerId; }

        void sendInput(const ClientInputPacket& packet);
        void sendChatMessage(const std::string& message);
        bool tryGetLatestWorldState(WorldStatePacket& outState);

        struct ChatMessage {
            std::uint32_t senderPlayerId{ 0 };
            std::string senderNick{};
            std::string message{};
        };

        std::vector<ChatMessage> getChatMessages();

    private:
        void receiveLoop();
        void addChatMessage(const ServerChatPacket& packet);

        bool sendAll(const char* data, int length);
        bool recvAll(char* data, int length);

        SOCKET sock = INVALID_SOCKET;
        std::thread receiveThread;

        std::atomic<bool> connected{ false };
        std::atomic<bool> stop{ false };
        std::atomic<std::uint32_t> localPlayerId{ 0 };

        std::mutex stateMutex;
        WorldStatePacket latestState{};
        bool hasState = false;

        std::mutex chatMutex;
        std::vector<ChatMessage> chatMessages{};
    };

} // namespace lve::net