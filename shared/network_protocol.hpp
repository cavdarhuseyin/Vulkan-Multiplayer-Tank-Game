#pragma once

#include <cstdint>
#include <cstddef>

namespace lve {
    namespace net {

        constexpr std::uint32_t kProtocolMagic = 0x54414E4B; // "TANK"
        constexpr std::uint32_t kProtocolVersion = 1;
        constexpr std::size_t kMaxPlayers = 8;

#pragma pack(push, 1)

        enum class PacketType : std::uint32_t {
            ClientHello = 1,
            ServerWelcome = 2,
            ClientInput = 3,
            WorldState = 4
        };

        struct ClientHelloPacket {
            std::uint32_t type{ static_cast<std::uint32_t>(PacketType::ClientHello) };
            std::uint32_t magic{ kProtocolMagic };
            std::uint32_t version{ kProtocolVersion };
        };

        struct ServerWelcomePacket {
            std::uint32_t type{ static_cast<std::uint32_t>(PacketType::ServerWelcome) };
            std::uint32_t playerId{ 0 };
        };

        struct ClientInputPacket {
            std::uint32_t type{ static_cast<std::uint32_t>(PacketType::ClientInput) };
            std::uint32_t inputSequence{ 0 };

            std::uint8_t moveForward{ 0 };
            std::uint8_t moveBackward{ 0 };
            std::uint8_t turnLeft{ 0 };
            std::uint8_t turnRight{ 0 };
            std::uint8_t turretLeft{ 0 };
            std::uint8_t turretRight{ 0 };
            std::uint8_t fire{ 0 };
            std::uint8_t reload{ 0 };
            std::uint8_t respawn{ 0 };

            std::uint8_t reserved[7]{};
        };

        struct PlayerStatePacket {
            std::uint32_t playerId{ 0 };

            float bodyPosX{ 0.f };
            float bodyPosY{ 0.f };
            float bodyPosZ{ 0.f };
            float bodyYaw{ 0.f };

            float turretYaw{ 0.f };

            float missilePosX{ 0.f };
            float missilePosY{ 0.f };
            float missilePosZ{ 0.f };

            std::uint8_t missileActive{ 0 };
            std::uint8_t missileDestroyed{ 0 };
            std::uint8_t alive{ 1 };
            std::uint8_t connected{ 0 };
        };

        struct WorldStatePacket {
            std::uint32_t type{ static_cast<std::uint32_t>(PacketType::WorldState) };
            std::uint32_t tick{ 0 };
            std::uint32_t playerCount{ 0 };
            PlayerStatePacket players[kMaxPlayers]{};
        };

#pragma pack(pop)

    } // namespace net
} // namespace lve
