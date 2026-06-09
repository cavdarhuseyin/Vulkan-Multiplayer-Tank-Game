#include "network_client.hpp"

#include <iostream>
#include <cstring>

namespace lve::net {

    namespace {
        std::string boundedString(const char* text, std::size_t maxLength) {
            std::size_t length = 0;
            while (length < maxLength && text[length] != '\0') {
                ++length;
            }
            return std::string(text, length);
        }

        template <std::size_t N>
        void copyToFixedBuffer(char (&dest)[N], const std::string& src) {
            std::memset(dest, 0, N);
            const std::size_t count = (src.size() < N - 1) ? src.size() : N - 1;
            if (count > 0) {
                std::memcpy(dest, src.data(), count);
            }
        }
    }

    NetworkClient::NetworkClient() {
        WSADATA wsaData{};
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    NetworkClient::~NetworkClient() {
        disconnect();
        WSACleanup();
    }

    bool NetworkClient::sendAll(const char* data, int length) {
        int totalSent = 0;
        while (totalSent < length) {
            int sent = send(sock, data + totalSent, length - totalSent, 0);
            if (sent == SOCKET_ERROR || sent == 0) {
                return false;
            }
            totalSent += sent;
        }
        return true;
    }

    bool NetworkClient::recvAll(char* data, int length) {
        int totalReceived = 0;
        while (totalReceived < length) {
            int received = recv(sock, data + totalReceived, length - totalReceived, 0);
            if (received == SOCKET_ERROR || received == 0) {
                return false;
            }
            totalReceived += received;
        }
        return true;
    }

    bool NetworkClient::connect(const std::string& host, int port, const std::string& nickname) {
        disconnect();

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            return false;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(static_cast<u_short>(port));

        if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) != 1) {
            closesocket(sock);
            sock = INVALID_SOCKET;
            return false;
        }

        if (::connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(sock);
            sock = INVALID_SOCKET;
            return false;
        }

        ClientHelloPacket hello{};
        copyToFixedBuffer(hello.nickname, nickname);

        if (!sendAll(reinterpret_cast<const char*>(&hello), sizeof(hello))) {
            disconnect();
            return false;
        }

        ServerWelcomePacket welcome{};
        if (!recvAll(reinterpret_cast<char*>(&welcome), sizeof(welcome))) {
            disconnect();
            return false;
        }

        if (welcome.type != static_cast<std::uint32_t>(PacketType::ServerWelcome)) {
            disconnect();
            return false;
        }

        localPlayerId = welcome.playerId;
        connected = true;
        stop = false;

        receiveThread = std::thread(&NetworkClient::receiveLoop, this);
        return true;
    }

    void NetworkClient::disconnect() {
        stop = true;
        connected = false;

        if (sock != INVALID_SOCKET) {
            shutdown(sock, SD_BOTH);
            closesocket(sock);
            sock = INVALID_SOCKET;
        }

        if (receiveThread.joinable()) {
            receiveThread.join();
        }
    }

    void NetworkClient::sendInput(const ClientInputPacket& packet) {
        if (!connected || sock == INVALID_SOCKET) return;

        if (!sendAll(reinterpret_cast<const char*>(&packet), sizeof(packet))) {
            connected = false;
        }
    }

    void NetworkClient::sendChatMessage(const std::string& message) {
        if (!connected || sock == INVALID_SOCKET || message.empty()) return;

        ClientChatPacket packet{};
        copyToFixedBuffer(packet.message, message);

        if (!sendAll(reinterpret_cast<const char*>(&packet), sizeof(packet))) {
            connected = false;
        }
    }

    bool NetworkClient::tryGetLatestWorldState(WorldStatePacket& outState) {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!hasState) return false;
        outState = latestState;
        return true;
    }

    std::vector<NetworkClient::ChatMessage> NetworkClient::getChatMessages() {
        std::lock_guard<std::mutex> lock(chatMutex);
        return chatMessages;
    }

    void NetworkClient::addChatMessage(const ServerChatPacket& packet) {
        ChatMessage message{};
        message.senderPlayerId = packet.senderPlayerId;
        message.senderNick = boundedString(packet.senderNick, kMaxNickLength);
        message.message = boundedString(packet.message, kMaxChatLength);

        {
            std::lock_guard<std::mutex> lock(chatMutex);
            chatMessages.push_back(message);
            if (chatMessages.size() > 8) {
                chatMessages.erase(chatMessages.begin());
            }
        }

        std::cout << "\n[" << message.senderNick << "]: " << message.message << "\n";
    }

    void NetworkClient::receiveLoop() {
        while (!stop && connected) {
            std::uint32_t packetType = 0;
            if (!recvAll(reinterpret_cast<char*>(&packetType), sizeof(packetType))) {
                connected = false;
                break;
            }

            if (packetType == static_cast<std::uint32_t>(PacketType::WorldState)) {
                WorldStatePacket packet{};
                packet.type = packetType;

                char* rest = reinterpret_cast<char*>(&packet) + sizeof(packetType);
                int restSize = static_cast<int>(sizeof(WorldStatePacket) - sizeof(packetType));

                if (!recvAll(rest, restSize)) {
                    connected = false;
                    break;
                }

                std::lock_guard<std::mutex> lock(stateMutex);
                latestState = packet;
                hasState = true;
            }
            else if (packetType == static_cast<std::uint32_t>(PacketType::ServerChat)) {
                ServerChatPacket packet{};
                packet.type = packetType;

                char* rest = reinterpret_cast<char*>(&packet) + sizeof(packetType);
                int restSize = static_cast<int>(sizeof(ServerChatPacket) - sizeof(packetType));

                if (!recvAll(rest, restSize)) {
                    connected = false;
                    break;
                }

                addChatMessage(packet);
            }
            else {
                connected = false;
                break;
            }
        }
    }

} // namespace lve::net