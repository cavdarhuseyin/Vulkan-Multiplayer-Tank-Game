#pragma once

#include "../shared/network_protocol.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

namespace lve::net {

    class NetworkClient {
    public:
        NetworkClient();
        ~NetworkClient();

        NetworkClient(const NetworkClient&) = delete;
        NetworkClient& operator=(const NetworkClient&) = delete;

        bool connect(const std::string& host, int port);
        void disconnect();

        bool isConnected() const { return connected; }
        std::uint32_t getLocalPlayerId() const { return localPlayerId; }

        void sendInput(const ClientInputPacket& packet);
        bool tryGetLatestWorldState(WorldStatePacket& outState);

    private:
        void receiveLoop();

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
    };

} // namespace lve::net