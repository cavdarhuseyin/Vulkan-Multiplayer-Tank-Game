#include "tank_server.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace lve {
    namespace net {

        namespace {
            float wrapAngle(float angle) {
                constexpr float twoPi = 6.28318530718f;
                while (angle < 0.f) angle += twoPi;
                while (angle >= twoPi) angle -= twoPi;
                return angle;
            }
        } // namespace

        TankServer::TankServer(unsigned short port)
            : port_(port) {
            WSADATA wsaData{};
            WSAStartup(MAKEWORD(2, 2), &wsaData);
        }

        TankServer::~TankServer() {
            stop();
            WSACleanup();
        }

        bool TankServer::sendAll(SOCKET s, const char* data, int length) {
            int totalSent = 0;
            while (totalSent < length) {
                int sent = send(s, data + totalSent, length - totalSent, 0);
                if (sent == SOCKET_ERROR || sent == 0) {
                    return false;
                }
                totalSent += sent;
            }
            return true;
        }

        bool TankServer::recvAll(SOCKET s, char* data, int length) {
            int totalReceived = 0;
            while (totalReceived < length) {
                int received = recv(s, data + totalReceived, length - totalReceived, 0);
                if (received == SOCKET_ERROR || received == 0) {
                    return false;
                }
                totalReceived += received;
            }
            return true;
        }

        void TankServer::run() {
            listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listenSocket_ == INVALID_SOCKET) {
                std::cerr << "Failed to create listen socket.\n";
                return;
            }

            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = INADDR_ANY;
            serverAddr.sin_port = htons(port_);

            if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
                std::cerr << "Bind failed.\n";
                closesocket(listenSocket_);
                listenSocket_ = INVALID_SOCKET;
                return;
            }

            if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
                std::cerr << "Listen failed.\n";
                closesocket(listenSocket_);
                listenSocket_ = INVALID_SOCKET;
                return;
            }

            running_ = true;
            acceptThread_ = std::thread(&TankServer::acceptLoop, this);

            auto previous = std::chrono::high_resolution_clock::now();
            constexpr float tickDt = 1.f / 30.f;

            while (running_) {
                auto now = std::chrono::high_resolution_clock::now();
                float elapsed = std::chrono::duration<float>(now - previous).count();

                if (elapsed < tickDt) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }

                previous = now;

                updateSimulation(tickDt);
                broadcastWorldState();
            }

            if (acceptThread_.joinable()) {
                acceptThread_.join();
            }

            std::vector<std::shared_ptr<ClientSession>> sessionsCopy;
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                for (auto& kv : sessions_) {
                    sessionsCopy.push_back(kv.second);
                }
                sessions_.clear();
                players_.clear();
            }

            for (auto& session : sessionsCopy) {
                shutdown(session->socket, SD_BOTH);
                closesocket(session->socket);

                if (session->readThread.joinable()) {
                    session->readThread.join();
                }
            }

            if (listenSocket_ != INVALID_SOCKET) {
                closesocket(listenSocket_);
                listenSocket_ = INVALID_SOCKET;
            }
        }

        void TankServer::stop() {
            running_ = false;

            if (listenSocket_ != INVALID_SOCKET) {
                shutdown(listenSocket_, SD_BOTH);
                closesocket(listenSocket_);
                listenSocket_ = INVALID_SOCKET;
            }
        }

        void TankServer::acceptLoop() {
            while (running_) {
                SOCKET clientSocket = accept(listenSocket_, nullptr, nullptr);
                if (clientSocket == INVALID_SOCKET) {
                    if (running_) {
                        std::cerr << "Accept failed.\n";
                    }
                    continue;
                }

                ClientHelloPacket hello{};
                if (!recvAll(clientSocket, reinterpret_cast<char*>(&hello), sizeof(hello))) {
                    closesocket(clientSocket);
                    continue;
                }

                if (hello.type != static_cast<std::uint32_t>(PacketType::ClientHello) ||
                    hello.magic != kProtocolMagic ||
                    hello.version != kProtocolVersion) {
                    closesocket(clientSocket);
                    continue;
                }

                auto session = std::make_shared<ClientSession>();
                session->socket = clientSocket;

                {
                    std::lock_guard<std::mutex> lock(stateMutex_);

                    if (players_.size() >= kMaxPlayers) {
                        closesocket(clientSocket);
                        continue;
                    }

                    session->playerId = nextPlayerId_++;

                    PlayerRuntimeState state{};
                    state.playerId = session->playerId;
                    state.bodyPosX = 8.0f + static_cast<float>((session->playerId - 1U) * 6U);
                    state.bodyPosY = 1.0f;
                    state.bodyPosZ = 20.0f;
                    state.bodyYaw = 0.f;
                    state.turretRelativeYaw = 0.f;
                    state.connected = true;
                    state.missileActive = false;

                    players_[session->playerId] = state;
                    sessions_[session->playerId] = session;
                }

                ServerWelcomePacket welcome{};
                welcome.playerId = session->playerId;

                if (!sendAll(session->socket, reinterpret_cast<const char*>(&welcome), sizeof(welcome))) {
                    removeClient(session->playerId);
                    continue;
                }

                session->readThread = std::thread(&TankServer::clientReadLoop, this, session);

                std::cout << "Client connected. playerId=" << session->playerId << "\n";
            }
        }

        void TankServer::clientReadLoop(std::shared_ptr<ClientSession> session) {
            while (running_) {
                ClientInputPacket input{};

                // Burada socket mutex kullanmýyoruz.
                // recv zaten bu thread'e özel; send tarafý broadcast thread'inde.
                if (!recvAll(session->socket, reinterpret_cast<char*>(&input), sizeof(input))) {
                    break;
                }

                if (input.type != static_cast<std::uint32_t>(PacketType::ClientInput)) {
                    break;
                }

                {
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    auto it = players_.find(session->playerId);
                    if (it != players_.end()) {
                        it->second.lastInput = input;
                    }
                }

                // Ýstersen geçici debug için açýk býrak
                // std::cout << "[Input] p=" << session->playerId
                //           << " F=" << static_cast<int>(input.moveForward)
                //           << " B=" << static_cast<int>(input.moveBackward)
                //           << " TL=" << static_cast<int>(input.turnLeft)
                //           << " TR=" << static_cast<int>(input.turnRight)
                //           << " TuL=" << static_cast<int>(input.turretLeft)
                //           << " TuR=" << static_cast<int>(input.turretRight)
                //           << " Fire=" << static_cast<int>(input.fire)
                //           << " Reload=" << static_cast<int>(input.reload)
                //           << "\n";
            }

            removeClient(session->playerId);
        }

        void TankServer::updateSimulation(float dt) {
            std::lock_guard<std::mutex> lock(stateMutex_);
            ++tickCounter_;

            constexpr float moveSpeed = 6.0f;
            constexpr float bodyTurnSpeed = 2.5f;
            constexpr float turretTurnSpeed = 1.8f;
            constexpr float missileSpeed = 30.0f;
            constexpr float barrelLength = 2.5f;
            constexpr float muzzleOffsetY = -1.0f;

            for (auto& kv : players_) {
                auto& player = kv.second;
                if (!player.connected) continue;

                const auto& in = player.lastInput;

                // Gövde dönüþü
                if (in.turnLeft)  player.bodyYaw -= bodyTurnSpeed * dt;
                if (in.turnRight) player.bodyYaw += bodyTurnSpeed * dt;
                player.bodyYaw = wrapAngle(player.bodyYaw);

                // Ýleri yönü client tarafýndaki forward hesabýyla uyumlu yap
                const float bodyForwardX = -std::sin(player.bodyYaw);
                const float bodyForwardZ = -std::cos(player.bodyYaw);

                float moveAxis = 0.f;
                if (in.moveForward)  moveAxis += 1.f;
                if (in.moveBackward) moveAxis -= 1.f;

                player.bodyPosX += bodyForwardX * moveAxis * moveSpeed * dt;
                player.bodyPosZ += bodyForwardZ * moveAxis * moveSpeed * dt;

                // Turret dönüþü
                if (in.turretLeft)  player.turretRelativeYaw += turretTurnSpeed * dt;
                if (in.turretRight) player.turretRelativeYaw -= turretTurnSpeed * dt;
                player.turretRelativeYaw = wrapAngle(player.turretRelativeYaw);

                const float turretYaw = wrapAngle(player.bodyYaw + player.turretRelativeYaw);
                const float turretForwardX = -std::sin(turretYaw);
                const float turretForwardZ = -std::cos(turretYaw);

                const float muzzlePosX = player.bodyPosX + turretForwardX * barrelLength;
                const float muzzlePosY = player.bodyPosY + muzzleOffsetY;
                const float muzzlePosZ = player.bodyPosZ + turretForwardZ * barrelLength;

                const bool reloadPressed = (in.reload != 0);
                const bool firePressed = (in.fire != 0);

                if (reloadPressed && !player.lastReloadDown) {
                    player.missileActive = false;
                    player.missilePosX = muzzlePosX;
                    player.missilePosY = muzzlePosY;
                    player.missilePosZ = muzzlePosZ;
                    player.missileVelX = 0.f;
                    player.missileVelY = 0.f;
                    player.missileVelZ = 0.f;
                }

                if (firePressed && !player.lastFireDown && !player.missileActive) {
                    player.missileActive = true;
                    player.missilePosX = muzzlePosX;
                    player.missilePosY = muzzlePosY;
                    player.missilePosZ = muzzlePosZ;
                    player.missileVelX = turretForwardX * missileSpeed;
                    player.missileVelY = 0.f;
                    player.missileVelZ = turretForwardZ * missileSpeed;
                }

                if (player.missileActive) {
                    player.missilePosX += player.missileVelX * dt;
                    player.missilePosY += player.missileVelY * dt;
                    player.missilePosZ += player.missileVelZ * dt;
                }
                else {
                    // Füze aktif deðilken namluda beklesin
                    player.missilePosX = muzzlePosX;
                    player.missilePosY = muzzlePosY;
                    player.missilePosZ = muzzlePosZ;
                }

                player.lastFireDown = firePressed;
                player.lastReloadDown = reloadPressed;
            }
        }

        WorldStatePacket TankServer::buildWorldStatePacket() const {
            WorldStatePacket packet{};
            packet.tick = tickCounter_;

            std::size_t index = 0;
            for (const auto& kv : players_) {
                if (index >= kMaxPlayers) break;

                const auto& p = kv.second;
                if (!p.connected) continue;

                auto& out = packet.players[index++];
                out.playerId = p.playerId;
                out.bodyPosX = p.bodyPosX;
                out.bodyPosY = p.bodyPosY;
                out.bodyPosZ = p.bodyPosZ;
                out.bodyYaw = p.bodyYaw;
                out.turretYaw = wrapAngle(p.bodyYaw + p.turretRelativeYaw);
                out.missilePosX = p.missilePosX;
                out.missilePosY = p.missilePosY;
                out.missilePosZ = p.missilePosZ;
                out.missileActive = static_cast<std::uint8_t>(p.missileActive ? 1 : 0);
                out.connected = static_cast<std::uint8_t>(p.connected ? 1 : 0);
            }

            packet.playerCount = static_cast<std::uint32_t>(index);
            return packet;
        }

        void TankServer::broadcastWorldState() {
            std::vector<std::shared_ptr<ClientSession>> sessionsCopy;
            WorldStatePacket packet{};

            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                packet = buildWorldStatePacket();

                for (auto& kv : sessions_) {
                    sessionsCopy.push_back(kv.second);
                }
            }

            std::vector<std::uint32_t> disconnectedIds;

            for (auto& session : sessionsCopy) {
                std::lock_guard<std::mutex> socketLock(session->socketMutex);
                if (!sendAll(session->socket, reinterpret_cast<const char*>(&packet), sizeof(packet))) {
                    disconnectedIds.push_back(session->playerId);
                }
            }

            for (auto playerId : disconnectedIds) {
                removeClient(playerId);
            }
        }

        void TankServer::removeClient(std::uint32_t playerId) {
            std::shared_ptr<ClientSession> session{};

            {
                std::lock_guard<std::mutex> lock(stateMutex_);

                auto sit = sessions_.find(playerId);
                if (sit != sessions_.end()) {
                    session = sit->second;
                    sessions_.erase(sit);
                }

                auto pit = players_.find(playerId);
                if (pit != players_.end()) {
                    players_.erase(pit);
                }
            }

            if (session) {
                shutdown(session->socket, SD_BOTH);
                closesocket(session->socket);

                if (session->readThread.joinable() &&
                    session->readThread.get_id() != std::this_thread::get_id()) {
                    session->readThread.join();
                }

                std::cout << "Client disconnected. playerId=" << playerId << "\n";
            }
        }

    } // namespace net
} // namespace lve