#pragma once

#define NOMINMAX

#include "../shared/network_protocol.hpp"
#include "../shared/server_mesh_collision.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace lve {
namespace net {

class TankServer {
public:
    explicit TankServer(unsigned short port);
    ~TankServer();

    TankServer(const TankServer&) = delete;
    TankServer& operator=(const TankServer&) = delete;

    void run();
    void stop();

private:
    struct PlayerRuntimeState {
        std::uint32_t playerId{0};

        float spawnPosX{0.f};
        float spawnPosY{1.f};
        float spawnPosZ{0.f};

        float bodyPosX{0.f};
        float bodyPosY{1.f};
        float bodyPosZ{0.f};
        float bodyYaw{0.f};

        float turretRelativeYaw{0.f};

        float missilePosX{0.f};
        float missilePosY{0.f};
        float missilePosZ{0.f};

        float missileVelX{0.f};
        float missileVelY{0.f};
        float missileVelZ{0.f};

        bool missileActive{false};
        bool missileDestroyed{false};
        bool alive{true};
        bool connected{true};

        ClientInputPacket lastInput{};
        bool lastFireDown{false};
        bool lastReloadDown{false};
        bool lastRespawnDown{false};
    };

    struct ClientSession {
        std::uint32_t playerId{0};
        SOCKET socket{INVALID_SOCKET};
        std::thread readThread;
        std::mutex socketMutex;
    };

private:
    void acceptLoop();
    void clientReadLoop(std::shared_ptr<ClientSession> session);

    void updateSimulation(float dt);
    void broadcastWorldState();
    WorldStatePacket buildWorldStatePacket() const;
    void removeClient(std::uint32_t playerId);

    bool sendAll(SOCKET s, const char* data, int length);
    bool recvAll(SOCKET s, char* data, int length);

    bool collidesWithOtherTank(
        std::uint32_t selfPlayerId,
        float x, float y, float z,
        float halfExtentX,
        float halfExtentY,
        float halfExtentZ) const;

    bool worldRayHit(
        const ServerMeshCollision::Vec3& origin,
        const ServerMeshCollision::Vec3& direction,
        float maxDistance,
        float& outDistance) const;

    bool hitTankWithMissile(
        std::uint32_t shooterPlayerId,
        float missileX,
        float missileY,
        float missileZ,
        std::uint32_t& hitPlayerId) const;

    void killPlayer(std::uint32_t playerId);
    void respawnPlayer(PlayerRuntimeState& player);

private:
    unsigned short port_{7777};
    SOCKET listenSocket_{INVALID_SOCKET};

    std::atomic<bool> running_{false};

    mutable std::mutex stateMutex_;
    std::unordered_map<std::uint32_t, PlayerRuntimeState> players_;
    std::unordered_map<std::uint32_t, std::shared_ptr<ClientSession>> sessions_;

    ServerMeshCollision cityCollision_{};

    std::uint32_t nextPlayerId_{1};
    std::uint32_t tickCounter_{0};

    std::thread acceptThread_;
};

} // namespace net
} // namespace lve
