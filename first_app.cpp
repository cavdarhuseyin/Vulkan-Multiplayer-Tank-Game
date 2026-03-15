#include "first_app.hpp"

#include <iostream>
#include <filesystem>
#include <unordered_set>
#include <limits>

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "keyboard_movement_controller.hpp"
#include "lve_buffer.hpp"
#include "lve_camera.hpp"
#include "lve_collision.hpp"
#include "simple_render_system.hpp"
#include "point_light_system.hpp"
#include "lve_texture.hpp"

// Libraries
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <stdexcept>
#include <array>
#include <chrono>
#include <memory>
#include <cmath>

namespace lve {

    namespace {
        constexpr const char* SERVER_IP = "127.0.0.1";
        constexpr int SERVER_PORT = 7777;
    }

    FirstApp::FirstApp() {
        globalPool = LveDescriptorPool::Builder(lveDevice)
            .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                LveSwapChain::MAX_FRAMES_IN_FLIGHT)
            .build();

        loadGameObjects();
    }

    FirstApp::~FirstApp() {
        networkClient.disconnect();
    }

    net::ClientInputPacket FirstApp::buildLocalInput(std::uint32_t inputSequence) const {
        net::ClientInputPacket input{};
        input.inputSequence = inputSequence;

        GLFWwindow* window = lveWindow.getGLFWwindow();

        input.moveForward = glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS ? 1 : 0;
        input.moveBackward = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS ? 1 : 0;
        input.turnLeft = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS ? 1 : 0;
        input.turnRight = glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS ? 1 : 0;
        input.turretLeft = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS ? 1 : 0;
        input.turretRight = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS ? 1 : 0;
        input.fire = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS ? 1 : 0;
        input.reload = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS ? 1 : 0;
        input.respawn = glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS ? 1 : 0;

        return input;
    }

    void FirstApp::ensurePlayerVisuals(
        std::uint32_t playerId,
        VkDescriptorSet tankTextureSet,
        VkDescriptorSet missileTextureSet) {

        if (playerVisuals.find(playerId) != playerVisuals.end()) {
            return;
        }

        PlayerVisualSet visuals{};

        // Tank body
        auto tankBody = LveGameObject::createGameObject();
        tankBody.model = tankBodyModel;
        tankBody.transform.translation = { 0.f, 1.f, 0.f };
        tankBody.transform.scale = { 1.0f, -1.0f, 1.0f };
        tankBody.textureDescriptorSet = tankTextureSet;
        visuals.tankBodyId = tankBody.getId();
        gameObjects.emplace(visuals.tankBodyId, std::move(tankBody));

        // Turret
        auto turret = LveGameObject::createGameObject();
        turret.model = tankTurretModel;
        turret.transform.translation = { 0.f, 1.f, 0.f };
        turret.transform.scale = { 1.0f, -1.0f, 1.0f };
        turret.textureDescriptorSet = tankTextureSet;
        visuals.turretId = turret.getId();
        gameObjects.emplace(visuals.turretId, std::move(turret));

        // Missile
        auto missile = LveGameObject::createGameObject();
        missile.model = missileModel;
        missile.transform.translation = { 0.f, -1000.f, 0.f };
        missile.transform.scale = { 1.0f, -1.0f, 1.0f };
        missile.textureDescriptorSet = missileTextureSet;
        visuals.missileId = missile.getId();
        gameObjects.emplace(visuals.missileId, std::move(missile));

        playerVisuals.emplace(playerId, visuals);

        if (playerId == networkClient.getLocalPlayerId()) {
            tankId = visuals.tankBodyId;
            turretId = visuals.turretId;
            missileId = visuals.missileId;
        }

        previousMissileActiveStates[playerId] = false;
    }

    void FirstApp::removeMissingPlayers(const net::WorldStatePacket& worldState) {
        std::unordered_set<std::uint32_t> alivePlayers;

        for (std::uint32_t i = 0; i < worldState.playerCount; i++) {
            if (worldState.players[i].connected) {
                alivePlayers.insert(worldState.players[i].playerId);
            }
        }

        std::vector<std::uint32_t> toRemove;
        for (const auto& kv : playerVisuals) {
            if (alivePlayers.find(kv.first) == alivePlayers.end()) {
                toRemove.push_back(kv.first);
            }
        }

        for (auto playerId : toRemove) {
            auto it = playerVisuals.find(playerId);
            if (it == playerVisuals.end()) continue;

            gameObjects.erase(it->second.tankBodyId);
            gameObjects.erase(it->second.turretId);
            gameObjects.erase(it->second.missileId);
            playerVisuals.erase(it);
            previousMissileActiveStates.erase(playerId);

            if (playerId == networkClient.getLocalPlayerId()) {
                tankId = {};
                turretId = {};
                missileId = {};
            }
        }
    }

    void FirstApp::applyWorldState(
        const net::WorldStatePacket& worldState,
        VkDescriptorSet tankTextureSet,
        VkDescriptorSet missileTextureSet) {

        removeMissingPlayers(worldState);

        for (std::uint32_t i = 0; i < worldState.playerCount; i++) {
            const auto& state = worldState.players[i];
            if (!state.connected) continue;

            const bool currentMissileActive = (state.missileActive != 0);
            bool previousMissileActive = false;

            auto prevIt = previousMissileActiveStates.find(state.playerId);
            if (prevIt != previousMissileActiveStates.end()) {
                previousMissileActive = prevIt->second;
            }

            if (!previousMissileActive && currentMissileActive) {
                PlaySound(TEXT("media\\tank_fire.wav"), NULL, SND_FILENAME | SND_ASYNC);
            }

            previousMissileActiveStates[state.playerId] = currentMissileActive;

            ensurePlayerVisuals(state.playerId, tankTextureSet, missileTextureSet);

            auto visualsIt = playerVisuals.find(state.playerId);
            if (visualsIt == playerVisuals.end()) continue;

            auto& visuals = visualsIt->second;

            // Tank body
            if (gameObjects.find(visuals.tankBodyId) != gameObjects.end()) {
                auto& tankBody = gameObjects.at(visuals.tankBodyId);
                if (state.alive) {
                    tankBody.transform.translation = { state.bodyPosX, state.bodyPosY, state.bodyPosZ };
                    tankBody.transform.rotation = { 0.f, state.bodyYaw, 0.f };
                    tankBody.transform.scale = { 1.0f, -1.0f, 1.0f };
                }
                else {
                    tankBody.transform.translation = { 0.f, -1000.f, 0.f };
                }
            }

            // Turret
            if (gameObjects.find(visuals.turretId) != gameObjects.end()) {
                auto& turret = gameObjects.at(visuals.turretId);
                if (state.alive) {
                    turret.transform.translation = { state.bodyPosX, state.bodyPosY, state.bodyPosZ };
                    turret.transform.rotation = { 0.f, state.turretYaw, 0.f };
                    turret.transform.scale = { 1.0f, -1.0f, 1.0f };
                }
                else {
                    turret.transform.translation = { 0.f, -1000.f, 0.f };
                }
            }

            // Missile
            if (gameObjects.find(visuals.missileId) != gameObjects.end()) {
                auto& missile = gameObjects.at(visuals.missileId);

                if (state.alive && !state.missileDestroyed) {
                    missile.transform.translation = {
                        state.missilePosX,
                        state.missilePosY,
                        state.missilePosZ
                    };

                    missile.transform.rotation = {
                        0.f,
                        state.turretYaw,
                        0.f
                    };

                    missile.transform.scale = { 1.0f, -1.0f, 1.0f };
                }
                else {
                    missile.transform.translation = { 0.f, -1000.f, 0.f };
                }
            }
        }

        // Local sight - multiplayer öncesindeki raycast mantığı
        auto localPlayerId = networkClient.getLocalPlayerId();
        auto localIt = playerVisuals.find(localPlayerId);
        bool localAlive = false;

        for (std::uint32_t i = 0; i < worldState.playerCount; i++) {
            if (worldState.players[i].playerId == localPlayerId) {
                localAlive = (worldState.players[i].alive != 0);
                break;
            }
        }

        if (localAlive &&
            localIt != playerVisuals.end() &&
            gameObjects.find(localIt->second.turretId) != gameObjects.end() &&
            gameObjects.find(sightId) != gameObjects.end()) {

            auto& localTurret = gameObjects.at(localIt->second.turretId);
            auto& sight = gameObjects.at(sightId);

            float yaw = localTurret.transform.rotation.y;

            glm::vec3 forward{
                -std::sin(yaw),
                0.f,
                -std::cos(yaw)
            };

            if (glm::length(forward) < 0.0001f) {
                forward = { 0.f, 0.f, -1.f };
            }
            else {
                forward = glm::normalize(forward);
            }

            const float barrelLength = 2.5f;
            const float muzzleOffsetY = -1.0f;

            glm::vec3 rayOrigin =
                localTurret.transform.translation +
                forward * barrelLength +
                glm::vec3{ 0.f, muzzleOffsetY, 0.f };

            float closestHit = (std::numeric_limits<float>::max)();
            bool hitFound = false;

            const auto localTankId = localIt->second.tankBodyId;
            const auto localTurretId = localIt->second.turretId;
            const auto localMissileId = localIt->second.missileId;

            for (auto& kv : gameObjects) {
                const auto objectId = kv.first;
                auto& obj = kv.second;

                if (!obj.model) continue;
                if (objectId == localTankId ||
                    objectId == localTurretId ||
                    objectId == localMissileId ||
                    objectId == sightId) {
                    continue;
                }

                const float hitT = LveCollision::IntersectModel(
                    rayOrigin,
                    forward,
                    *obj.model,
                    obj.transform.mat4()
                );

                if (hitT > 0.f && hitT < closestHit && hitT < 1000.f) {
                    closestHit = hitT;
                    hitFound = true;
                }
            }

            if (hitFound) {
                glm::vec3 hitPoint = rayOrigin + forward * closestHit;
                sight.transform.translation = hitPoint - forward * 0.05f;
            }
            else {
                sight.transform.translation = rayOrigin + forward * 50.0f;
            }

            sight.transform.rotation = { 0.f, yaw + glm::pi<float>(), 0.f };
            sight.transform.scale = { 3.f, 3.f, 3.f };
        }
        else if (gameObjects.find(sightId) != gameObjects.end()) {
            gameObjects.at(sightId).transform.translation = { 0.f, -1000.f, 0.f };
        }
    }

    void FirstApp::run() {
        std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < static_cast<int>(uboBuffers.size()); i++) {
            uboBuffers[i] = std::make_unique<LveBuffer>(
                lveDevice,
                sizeof(GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            uboBuffers[i]->map();
        }

        auto globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();

        std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < static_cast<int>(globalDescriptorSets.size()); i++) {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            LveDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .build(globalDescriptorSets[i]);
        }

        auto textureSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();

        auto texturePool = LveDescriptorPool::Builder(lveDevice)
            .setMaxSets(4096)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096)
            .build();

        auto tankTexture = std::make_shared<LveTexture>(lveDevice, "textures/Tank.png");
        VkDescriptorSet tankTextureSet{ VK_NULL_HANDLE };
        VkDescriptorImageInfo tankImageInfo{};
        tankImageInfo.sampler = tankTexture->sampler();
        tankImageInfo.imageView = tankTexture->imageView();
        tankImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        LveDescriptorWriter(*textureSetLayout, *texturePool)
            .writeImage(0, &tankImageInfo)
            .build(tankTextureSet);

        auto missileTexture = std::make_shared<LveTexture>(lveDevice, "textures/red.png");
        VkDescriptorSet missileTextureSet{ VK_NULL_HANDLE };
        VkDescriptorImageInfo missileImageInfo{};
        missileImageInfo.sampler = missileTexture->sampler();
        missileImageInfo.imageView = missileTexture->imageView();
        missileImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        LveDescriptorWriter(*textureSetLayout, *texturePool)
            .writeImage(0, &missileImageInfo)
            .build(missileTextureSet);

        auto sightTexture = std::make_shared<LveTexture>(lveDevice, "textures/red.png");
        VkDescriptorSet sightTextureSet{ VK_NULL_HANDLE };
        VkDescriptorImageInfo sightImageInfo{};
        sightImageInfo.sampler = sightTexture->sampler();
        sightImageInfo.imageView = sightTexture->imageView();
        sightImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        LveDescriptorWriter(*textureSetLayout, *texturePool)
            .writeImage(0, &sightImageInfo)
            .build(sightTextureSet);

        auto cityTexture = std::make_shared<LveTexture>(lveDevice, "textures/City.jpg");
        VkDescriptorSet cityTextureSet{ VK_NULL_HANDLE };
        VkDescriptorImageInfo cityImageInfo{};
        cityImageInfo.sampler = cityTexture->sampler();
        cityImageInfo.imageView = cityTexture->imageView();
        cityImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        LveDescriptorWriter(*textureSetLayout, *texturePool)
            .writeImage(0, &cityImageInfo)
            .build(cityTextureSet);

        auto groundTexture = std::make_shared<LveTexture>(lveDevice, "textures/Marble.jpg");
        VkDescriptorSet groundTextureSet{ VK_NULL_HANDLE };
        VkDescriptorImageInfo groundImageInfo{};
        groundImageInfo.sampler = groundTexture->sampler();
        groundImageInfo.imageView = groundTexture->imageView();
        groundImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        LveDescriptorWriter(*textureSetLayout, *texturePool)
            .writeImage(0, &groundImageInfo)
            .build(groundTextureSet);

        if (gameObjects.find(townId) != gameObjects.end()) {
            gameObjects.at(townId).textureDescriptorSet = cityTextureSet;
        }
        if (gameObjects.find(groundId) != gameObjects.end()) {
            gameObjects.at(groundId).textureDescriptorSet = groundTextureSet;
        }
        if (gameObjects.find(sightId) != gameObjects.end()) {
            gameObjects.at(sightId).textureDescriptorSet = sightTextureSet;
        }

        SimpleRenderSystem simpleRenderSystem{
            lveDevice,
            lveRenderer.getSwapChainRenderPass(),
            globalSetLayout->getDescriptorSetLayout(),
            textureSetLayout->getDescriptorSetLayout()
        };

        PointLightSystem pointLightSystem{
            lveDevice,
            lveRenderer.getSwapChainRenderPass(),
            globalSetLayout->getDescriptorSetLayout()
        };

        LveCamera camera{};
        auto viewerObject = LveGameObject::createGameObject();
        viewerObject.transform.translation = { 0.f, -4.f, -8.f };

        KeyboardMovementController cameraController{};
        cameraController.moveSpeed = 10.f;
        cameraController.lookSpeed = 1.5f;

        auto currentTime = std::chrono::high_resolution_clock::now();

        if (!networkClient.connect(SERVER_IP, SERVER_PORT)) {
            std::cerr << "Server'a baglanilamadi: " << SERVER_IP << ":" << SERVER_PORT << "\n";
        }
        else {
            std::cout << "Server baglandi. Local playerId = "
                << networkClient.getLocalPlayerId() << "\n";
        }

        while (!lveWindow.shouldClose()) {
            glfwPollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime =
                std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;
            frameTime = (glm::min)(frameTime, 0.1f);

            if (glfwGetKey(lveWindow.getGLFWwindow(), GLFW_KEY_V) == GLFW_PRESS) {
                if (!fpsTogglePressed) {
                    fpsMode = !fpsMode;
                    fpsTogglePressed = true;
                }
            }
            else {
                fpsTogglePressed = false;
            }

            if (!fpsMode) {
                cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
            }

            if (networkClient.isConnected()) {
                auto input = buildLocalInput(++inputSequenceCounter);
                networkClient.sendInput(input);
            }

            net::WorldStatePacket latestWorldState{};
            if (networkClient.tryGetLatestWorldState(latestWorldState)) {
                applyWorldState(latestWorldState, tankTextureSet, missileTextureSet);
            }

            bool fpsCameraApplied = false;
            auto localPlayerId = networkClient.getLocalPlayerId();
            auto localIt = playerVisuals.find(localPlayerId);

            if (fpsMode &&
                localIt != playerVisuals.end() &&
                gameObjects.find(localIt->second.turretId) != gameObjects.end()) {

                auto& tankTurret = gameObjects.at(localIt->second.turretId);
                float yaw = tankTurret.transform.rotation.y;

                glm::vec3 forward{
                    -std::sin(yaw),
                    0.f,
                    -std::cos(yaw)
                };

                if (glm::length(forward) < 0.0001f) {
                    forward = { 0.f, 0.f, -1.f };
                }
                else {
                    forward = glm::normalize(forward);
                }

                float forwardOffset = -1.15f;
                float upOffset = -2.00f;

                glm::vec3 cameraPos =
                    tankTurret.transform.translation +
                    forward * forwardOffset +
                    glm::vec3{ 0.f, upOffset, 0.f };

                glm::vec3 targetPos = cameraPos + forward * 10.0f;

                camera.setViewTarget(cameraPos, targetPos);
                fpsCameraApplied = true;
            }

            if (!fpsCameraApplied) {
                camera.setViewYXZ(
                    viewerObject.transform.translation,
                    viewerObject.transform.rotation
                );
            }

            float aspect = lveRenderer.getAspectRatio();
            camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.5f, 500.f);

            if (auto commandBuffer = lveRenderer.beginFrame()) {
                int frameIndex = lveRenderer.getFrameIndex();
                FrameInfo frameInfo{
                    frameIndex,
                    frameTime,
                    commandBuffer,
                    camera,
                    globalDescriptorSets[frameIndex],
                    gameObjects
                };

                GlobalUbo ubo{};
                ubo.projection = camera.getProjectionMatrix();
                ubo.view = camera.getViewMatrix();
                ubo.inverseView = camera.getInverseViewMatrix();

                pointLightSystem.update(frameInfo, ubo);

                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                lveRenderer.beginSwapChainRenderPass(commandBuffer);
                simpleRenderSystem.renderGameObjects(frameInfo);
                pointLightSystem.render(frameInfo);
                lveRenderer.endSwapChainRenderPass(commandBuffer);
                lveRenderer.endFrame();
            }
        }

        vkDeviceWaitIdle(lveDevice.device());
    }

    void FirstApp::loadGameObjects() {
        tankBodyModel = LveModel::createModelFromFile(lveDevice, "models/TankBody.obj");
        tankTurretModel = LveModel::createModelFromFile(lveDevice, "models/TankTurret+Barrel.obj");
        missileModel = LveModel::createModelFromFile(lveDevice, "models/Missile.obj");
        townModel = LveModel::createModelFromFile(lveDevice, "models/City.obj");
        groundModel = LveModel::createModelFromFile(lveDevice, "models/Ground.obj");
        sightModel = LveModel::createModelFromFile(lveDevice, "models/RedDot.obj");

        auto town = LveGameObject::createGameObject();
        town.model = townModel;
        town.transform.translation = { 0.f, 1.0f, 0.f };
        town.transform.scale = { 0.5f, -0.5f, 0.5f };
        townId = town.getId();
        gameObjects.emplace(townId, std::move(town));

        auto ground = LveGameObject::createGameObject();
        ground.model = groundModel;
        ground.transform.translation = { 0.f, 1.0f, 0.f };
        ground.transform.scale = { 0.5f, -0.5f, 0.5f };
        groundId = ground.getId();
        gameObjects.emplace(groundId, std::move(ground));

        auto sight = LveGameObject::createGameObject();
        sight.model = sightModel;
        sight.transform.scale = { 3.f, 3.f, 3.f };
        sightId = sight.getId();
        gameObjects.emplace(sightId, std::move(sight));
    }

} // namespace lve

