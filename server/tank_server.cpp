#include "tank_server.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <cstring>
#include <limits>
#include <string>

namespace lve {
    namespace net {

        namespace {

            std::string boundedString(const char* text, std::size_t maxLength) {
                std::size_t length = 0;

                while (length < maxLength && text[length] != '\0') {
                    ++length;
                }

                return std::string(text, length);
            }

            template <std::size_t N>
            void copyToFixedBuffer(char(&dest)[N], const std::string& src) {
                std::memset(dest, 0, N);                 // Hedef karakter dizisini sýfýrlar

                strncpy_s(                               // Visual Studio için güvenli kopyalama fonksiyonu
                    dest,                                // Kopyalanacak hedef char dizisi
                    N,                                   // Hedef dizinin toplam boyutu
                    src.c_str(),                         // std::string verisini C-string'e çevirir
                    N - 1                                // En fazla N-1 karakter kopyalar, son karakter '\0' kalýr
                );
            }

            float wrapAngle(float angle) {
                constexpr float twoPi = 6.28318530718f;

                while (angle < 0.f) {
                    angle += twoPi;
                }

                while (angle >= twoPi) {
                    angle -= twoPi;
                }

                return angle;
            }

        } // namespace

        TankServer::TankServer(unsigned short port)
            : port_(port) {
            WSADATA wsaData{};
            WSAStartup(MAKEWORD(2, 2), &wsaData);

            const bool loaded = cityCollision_.loadObj(
                "models/City.obj",
                { 0.0f, 1.0f, 0.0f },
                { 0.5f, -0.5f, 0.5f });

            std::cout << "[TankServer] City mesh collision load: "
                << (loaded ? "OK" : "FAILED") << "\n";
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

        bool TankServer::collidesWithOtherTank(
            std::uint32_t selfPlayerId,
            float x,
            float y,
            float z,
            float halfExtentX,
            float halfExtentY,
            float halfExtentZ) const {

            const float minX = x - halfExtentX;
            const float maxX = x + halfExtentX;
            const float minY = y - halfExtentY;
            const float maxY = y + halfExtentY;
            const float minZ = z - halfExtentZ;
            const float maxZ = z + halfExtentZ;

            for (const auto& kv : players_) {
                const auto& other = kv.second;

                if (!other.connected || !other.alive || other.playerId == selfPlayerId) {
                    continue;
                }

                constexpr float otherHalfExtentX = 1.4f;
                constexpr float otherHalfExtentY = 1.2f;
                constexpr float otherHalfExtentZ = 1.8f;

                const float otherMinX = other.bodyPosX - otherHalfExtentX;
                const float otherMaxX = other.bodyPosX + otherHalfExtentX;
                const float otherMinY = other.bodyPosY - otherHalfExtentY;
                const float otherMaxY = other.bodyPosY + otherHalfExtentY;
                const float otherMinZ = other.bodyPosZ - otherHalfExtentZ;
                const float otherMaxZ = other.bodyPosZ + otherHalfExtentZ;

                const bool overlap =
                    (minX <= otherMaxX && maxX >= otherMinX) &&
                    (minY <= otherMaxY && maxY >= otherMinY) &&
                    (minZ <= otherMaxZ && maxZ >= otherMinZ);

                if (overlap) {
                    return true;
                }
            }

            return false;
        }

        bool TankServer::worldRayHit(
            const ServerMeshCollision::Vec3& origin,
            const ServerMeshCollision::Vec3& direction,
            float maxDistance,
            float& outDistance) const {

            return cityCollision_.raycast(origin, direction, maxDistance, outDistance);
        }

        bool TankServer::hitTankWithMissile(
            std::uint32_t shooterPlayerId,
            float missileX,
            float missileY,
            float missileZ,
            std::uint32_t& hitPlayerId) const {

            constexpr float tankHalfExtentX = 1.4f;
            constexpr float tankHalfExtentY = 1.2f;
            constexpr float tankHalfExtentZ = 1.8f;

            for (const auto& kv : players_) {
                const auto& other = kv.second;

                if (!other.connected) {
                    continue;
                }

                if (!other.alive) {
                    continue;
                }

                if (other.playerId == shooterPlayerId) {
                    continue;
                }

                if (missileX >= other.bodyPosX - tankHalfExtentX &&
                    missileX <= other.bodyPosX + tankHalfExtentX &&
                    missileY >= other.bodyPosY - tankHalfExtentY &&
                    missileY <= other.bodyPosY + tankHalfExtentY &&
                    missileZ >= other.bodyPosZ - tankHalfExtentZ &&
                    missileZ <= other.bodyPosZ + tankHalfExtentZ) {

                    hitPlayerId = other.playerId;
                    return true;
                }
            }

            return false;
        }

        void TankServer::killPlayer(std::uint32_t playerId) {
            auto it = players_.find(playerId);

            if (it == players_.end()) {
                return;
            }

            auto& player = it->second;

            player.alive = false;
            player.missileActive = false;
            player.missileDestroyed = true;

            player.missileVelX = 0.f;
            player.missileVelY = 0.f;
            player.missileVelZ = 0.f;

            player.missilePosX = 0.f;
            player.missilePosY = -1000.f;
            player.missilePosZ = 0.f;

            player.bodyPosX = 0.f;
            player.bodyPosY = -1000.f;
            player.bodyPosZ = 0.f;
        }

        void TankServer::respawnPlayer(PlayerRuntimeState& player) {
            player.alive = true;

            player.bodyPosX = player.spawnPosX;
            player.bodyPosY = player.spawnPosY;
            player.bodyPosZ = player.spawnPosZ;

            player.bodyYaw = 0.f;
            player.turretRelativeYaw = 0.f;

            player.missileActive = false;
            player.missileDestroyed = false;

            player.missileVelX = 0.f;
            player.missileVelY = 0.f;
            player.missileVelZ = 0.f;
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
                {
                    std::lock_guard<std::mutex> socketLock(session->socketMutex);

                    if (session->socket != INVALID_SOCKET) {
                        shutdown(session->socket, SD_BOTH);
                        closesocket(session->socket);
                        session->socket = INVALID_SOCKET;
                    }
                }

                if (session->readThread.joinable()) {
                    if (session->readThread.get_id() == std::this_thread::get_id()) {
                        session->readThread.detach();
                    }
                    else {
                        session->readThread.join();
                    }
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

                    state.nickname = boundedString(hello.nickname, kMaxNickLength);

                    if (state.nickname.empty()) {
                        state.nickname = "Player" + std::to_string(session->playerId);
                    }

                    state.spawnPosX = 8.0f + static_cast<float>((session->playerId - 1U) * 6U);
                    state.spawnPosY = 1.0f;
                    state.spawnPosZ = 30.0f;

                    state.bodyPosX = state.spawnPosX;
                    state.bodyPosY = state.spawnPosY;
                    state.bodyPosZ = state.spawnPosZ;

                    state.bodyYaw = 0.f;
                    state.turretRelativeYaw = 0.f;

                    state.connected = true;
                    state.alive = true;

                    state.missileActive = false;
                    state.missileDestroyed = false;

                    players_[session->playerId] = state;
                }

                ServerWelcomePacket welcome{};
                welcome.playerId = session->playerId;

                if (!sendAll(session->socket, reinterpret_cast<const char*>(&welcome), sizeof(welcome))) {
                    removeClient(session->playerId);
                    continue;
                }

                session->readThread = std::thread(&TankServer::clientReadLoop, this, session);

                {
                    std::lock_guard<std::mutex> lock(stateMutex_);

                    auto pit = players_.find(session->playerId);

                    if (pit != players_.end()) {
                        sessions_[session->playerId] = session;
                    }
                    else {
                        if (session->readThread.joinable()) {
                            session->readThread.detach();
                        }

                        continue;
                    }
                }

                std::cout << "Client connected. playerId=" << session->playerId
                    << " nick=" << boundedString(hello.nickname, kMaxNickLength) << "\n";
            }
        }

        void TankServer::clientReadLoop(std::shared_ptr<ClientSession> session) {
            while (running_) {
                std::uint32_t packetType = 0;

                if (!recvAll(session->socket, reinterpret_cast<char*>(&packetType), sizeof(packetType))) {
                    break;
                }

                if (packetType == static_cast<std::uint32_t>(PacketType::ClientInput)) {
                    ClientInputPacket input{};
                    input.type = packetType;

                    char* rest = reinterpret_cast<char*>(&input) + sizeof(packetType);
                    int restSize = static_cast<int>(sizeof(ClientInputPacket) - sizeof(packetType));

                    if (!recvAll(session->socket, rest, restSize)) {
                        break;
                    }

                    std::lock_guard<std::mutex> lock(stateMutex_);

                    auto it = players_.find(session->playerId);

                    if (it != players_.end()) {
                        it->second.lastInput = input;
                    }
                }
                else if (packetType == static_cast<std::uint32_t>(PacketType::ClientChat)) {
                    ClientChatPacket chat{};
                    chat.type = packetType;

                    char* rest = reinterpret_cast<char*>(&chat) + sizeof(packetType);
                    int restSize = static_cast<int>(sizeof(ClientChatPacket) - sizeof(packetType));

                    if (!recvAll(session->socket, rest, restSize)) {
                        break;
                    }

                    broadcastChatMessage(session->playerId, chat.message);
                }
                else {
                    break;
                }
            }

            removeClient(session->playerId);
        }

        void TankServer::updateSimulation(float dt) {
            std::lock_guard<std::mutex> lock(stateMutex_);

            ++tickCounter_;

            constexpr float moveSpeed = 3.5f;
            constexpr float bodyTurnSpeed = 2.5f;
            constexpr float turretTurnSpeed = 1.8f;
            constexpr float missileSpeed = 30.0f;
            constexpr float barrelLength = 2.5f;
            constexpr float muzzleOffsetY = -1.0f;

            constexpr float tankHalfExtentX = 1.4f;
            constexpr float tankHalfExtentY = 1.2f;
            constexpr float tankHalfExtentZ = 1.8f;

            for (auto& kv : players_) {
                auto& player = kv.second;

                if (!player.connected) {
                    continue;
                }

                const auto& in = player.lastInput;
                const bool respawnPressed = (in.respawn != 0);

                if (!player.alive) {
                    if (respawnPressed && !player.lastRespawnDown) {
                        respawnPlayer(player);
                    }

                    player.lastRespawnDown = respawnPressed;
                    player.lastFireDown = (in.fire != 0);
                    player.lastReloadDown = (in.reload != 0);

                    continue;
                }

                if (in.turnLeft) {
                    player.bodyYaw -= bodyTurnSpeed * dt;
                }

                if (in.turnRight) {
                    player.bodyYaw += bodyTurnSpeed * dt;
                }

                player.bodyYaw = wrapAngle(player.bodyYaw);

                const float bodyForwardX = -std::sin(player.bodyYaw);
                const float bodyForwardZ = -std::cos(player.bodyYaw);

                float moveAxis = 0.f;

                if (in.moveForward) {
                    moveAxis += 1.f;
                }

                if (in.moveBackward) {
                    moveAxis -= 1.f;
                }

                if (std::fabs(moveAxis) > 0.0001f) {
                    const ServerMeshCollision::Vec3 rayOrigin{
                        player.bodyPosX,
                        player.bodyPosY - 1.0f,
                        player.bodyPosZ
                    };

                    const ServerMeshCollision::Vec3 rayDir{
                        bodyForwardX * moveAxis,
                        0.f,
                        bodyForwardZ * moveAxis
                    };

                    float hitDistance = (std::numeric_limits<float>::max)();

                    const bool hitsWorld = worldRayHit(
                        rayOrigin,
                        rayDir,
                        2.5f,
                        hitDistance
                    );

                    const float nextBodyPosX = player.bodyPosX + bodyForwardX * moveAxis * moveSpeed * dt;
                    const float nextBodyPosZ = player.bodyPosZ + bodyForwardZ * moveAxis * moveSpeed * dt;

                    const bool hitsOtherTank = collidesWithOtherTank(
                        player.playerId,
                        nextBodyPosX,
                        player.bodyPosY,
                        nextBodyPosZ,
                        tankHalfExtentX,
                        tankHalfExtentY,
                        tankHalfExtentZ
                    );

                    if (!hitsWorld && !hitsOtherTank) {
                        player.bodyPosX = nextBodyPosX;
                        player.bodyPosZ = nextBodyPosZ;
                    }
                }

                if (in.turretLeft) {
                    player.turretRelativeYaw += turretTurnSpeed * dt;
                }

                if (in.turretRight) {
                    player.turretRelativeYaw -= turretTurnSpeed * dt;
                }

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
                    player.missileDestroyed = false;

                    player.missilePosX = muzzlePosX;
                    player.missilePosY = muzzlePosY;
                    player.missilePosZ = muzzlePosZ;

                    player.missileVelX = 0.f;
                    player.missileVelY = 0.f;
                    player.missileVelZ = 0.f;
                }

                if (firePressed && !player.lastFireDown && !player.missileActive && !player.missileDestroyed) {
                    player.missileActive = true;

                    player.missilePosX = muzzlePosX;
                    player.missilePosY = muzzlePosY;
                    player.missilePosZ = muzzlePosZ;

                    player.missileVelX = turretForwardX * missileSpeed;
                    player.missileVelY = 0.f;
                    player.missileVelZ = turretForwardZ * missileSpeed;
                }

                if (player.missileActive) {
                    const float moveDist = missileSpeed * dt;

                    const ServerMeshCollision::Vec3 missileOrigin{
                        player.missilePosX,
                        player.missilePosY,
                        player.missilePosZ
                    };

                    const ServerMeshCollision::Vec3 missileDir{
                        player.missileVelX,
                        player.missileVelY,
                        player.missileVelZ
                    };

                    float hitDistance = (std::numeric_limits<float>::max)();

                    const float nextMissileX = player.missilePosX + player.missileVelX * dt;
                    const float nextMissileY = player.missilePosY + player.missileVelY * dt;
                    const float nextMissileZ = player.missilePosZ + player.missileVelZ * dt;

                    std::uint32_t hitPlayerId = 0;

                    if (hitTankWithMissile(
                        player.playerId,
                        nextMissileX,
                        nextMissileY,
                        nextMissileZ,
                        hitPlayerId)) {

                        player.missileActive = false;
                        player.missileDestroyed = true;

                        player.missileVelX = 0.f;
                        player.missileVelY = 0.f;
                        player.missileVelZ = 0.f;

                        player.missilePosX = 0.f;
                        player.missilePosY = -1000.f;
                        player.missilePosZ = 0.f;

                        killPlayer(hitPlayerId);
                    }
                    else if (worldRayHit(missileOrigin, missileDir, moveDist, hitDistance)) {
                        player.missileActive = false;
                        player.missileDestroyed = true;

                        player.missileVelX = 0.f;
                        player.missileVelY = 0.f;
                        player.missileVelZ = 0.f;

                        player.missilePosX = 0.f;
                        player.missilePosY = -1000.f;
                        player.missilePosZ = 0.f;
                    }
                    else {
                        player.missilePosX = nextMissileX;
                        player.missilePosY = nextMissileY;
                        player.missilePosZ = nextMissileZ;
                    }
                }
                else if (!player.missileDestroyed) {
                    player.missilePosX = muzzlePosX;
                    player.missilePosY = muzzlePosY;
                    player.missilePosZ = muzzlePosZ;
                }

                player.lastFireDown = firePressed;
                player.lastReloadDown = reloadPressed;
                player.lastRespawnDown = respawnPressed;
            }
        }

        WorldStatePacket TankServer::buildWorldStatePacket() const {
            WorldStatePacket packet{};
            packet.tick = tickCounter_;

            std::size_t index = 0;

            for (const auto& kv : players_) {
                if (index >= kMaxPlayers) {
                    break;
                }

                const auto& p = kv.second;

                if (!p.connected) {
                    continue;
                }

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
                out.missileDestroyed = static_cast<std::uint8_t>(p.missileDestroyed ? 1 : 0);

                out.alive = static_cast<std::uint8_t>(p.alive ? 1 : 0);
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

        void TankServer::broadcastChatMessage(std::uint32_t senderPlayerId, const char* message) {
            std::vector<std::shared_ptr<ClientSession>> sessionsCopy;
            ServerChatPacket packet{};

            {
                std::lock_guard<std::mutex> lock(stateMutex_);

                auto senderIt = players_.find(senderPlayerId);

                if (senderIt == players_.end()) {
                    return;
                }

                packet.senderPlayerId = senderPlayerId;

                copyToFixedBuffer(packet.senderNick, senderIt->second.nickname);
                copyToFixedBuffer(packet.message, boundedString(message, kMaxChatLength));

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
                {
                    std::lock_guard<std::mutex> socketLock(session->socketMutex);

                    if (session->socket != INVALID_SOCKET) {
                        shutdown(session->socket, SD_BOTH);
                        closesocket(session->socket);
                        session->socket = INVALID_SOCKET;
                    }
                }

                if (session->readThread.joinable()) {
                    if (session->readThread.get_id() == std::this_thread::get_id()) {
                        session->readThread.detach();
                    }
                    else {
                        session->readThread.join();
                    }
                }

                std::cout << "Client disconnected. playerId=" << playerId << "\n";
            }
        }

    } // namespace net
} // namespace lve