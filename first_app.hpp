#pragma once

#include "lve_descriptors.hpp"
#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_game_object.hpp"
#include "lve_renderer.hpp"
#include "client/network_client.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace lve {

    class FirstApp {
    public:
        static constexpr int WIDTH = 800;
        static constexpr int HEIGHT = 600;

        FirstApp();
        ~FirstApp();

        FirstApp(const FirstApp&) = delete;
        FirstApp& operator=(const FirstApp&) = delete;

        void run();

    private:
        struct PlayerVisualSet {
            LveGameObject::id_t tankBodyId{};
            LveGameObject::id_t turretId{};
            LveGameObject::id_t missileId{};
        };

        void loadGameObjects();

        void ensurePlayerVisuals(
            std::uint32_t playerId,
            VkDescriptorSet tankTextureSet,
            VkDescriptorSet missileTextureSet);

        void removeMissingPlayers(const net::WorldStatePacket& worldState);
        void applyWorldState(
            const net::WorldStatePacket& worldState,
            VkDescriptorSet tankTextureSet,
            VkDescriptorSet missileTextureSet);

        net::ClientInputPacket buildLocalInput(std::uint32_t inputSequence) const;

        LveWindow lveWindow{ WIDTH, HEIGHT, "Hello Vulkan!" };
        LveDevice lveDevice{ lveWindow };
        LveRenderer lveRenderer{ lveWindow, lveDevice };

        std::unique_ptr<LveDescriptorPool> globalPool{};
        LveGameObject::Map gameObjects;

        // Object IDs (environment + local player fallback)
        LveGameObject::id_t tankId{};
        LveGameObject::id_t turretId{};
        LveGameObject::id_t missileId{};
        LveGameObject::id_t townId{};
        LveGameObject::id_t groundId{};
        LveGameObject::id_t sightId{};

        // Shared models
        std::shared_ptr<LveModel> tankBodyModel{};
        std::shared_ptr<LveModel> tankTurretModel{};
        std::shared_ptr<LveModel> missileModel{};
        std::shared_ptr<LveModel> townModel{};
        std::shared_ptr<LveModel> groundModel{};
        std::shared_ptr<LveModel> sightModel{};

        // Multiplayer
        net::NetworkClient networkClient{};
        std::unordered_map<std::uint32_t, PlayerVisualSet> playerVisuals{};

        // Camera state
        bool fpsMode{ false };
        bool fpsTogglePressed{ false };

        // Network state
        std::uint32_t inputSequenceCounter{ 0 };
    };

} // namespace lve