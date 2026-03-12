#include "network_client.hpp"

#include <iostream>

namespace lve::net {

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

    bool NetworkClient::connect(const std::string& host, int port) {
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

    bool NetworkClient::tryGetLatestWorldState(WorldStatePacket& outState) {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!hasState) return false;
        outState = latestState;
        return true;
    }

    void NetworkClient::receiveLoop() {
        while (!stop && connected) {
            WorldStatePacket packet{};
            if (!recvAll(reinterpret_cast<char*>(&packet), sizeof(packet))) {
                connected = false;
                break;
            }

            if (packet.type != static_cast<std::uint32_t>(PacketType::WorldState)) {
                connected = false;
                break;
            }

            std::lock_guard<std::mutex> lock(stateMutex);
            latestState = packet;
            hasState = true;
        }
    }

} // namespace lve::net