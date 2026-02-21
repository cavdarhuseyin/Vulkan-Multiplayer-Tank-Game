#include "first_app.hpp"

#include <iostream>
#include <filesystem>
#include "keyboard_movement_controller.hpp"
#include "lve_buffer.hpp"
#include "lve_camera.hpp"
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

namespace lve {

FirstApp::FirstApp() {
  globalPool = LveDescriptorPool::Builder(lveDevice)
                   .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
                   .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                LveSwapChain::MAX_FRAMES_IN_FLIGHT)
                   .build();

  loadGameObjects();
}

FirstApp::~FirstApp() {}

void FirstApp::run() {
  
  std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < static_cast<int>(uboBuffers.size()); i++) {
    uboBuffers[i] = std::make_unique<LveBuffer>(
        lveDevice, sizeof(GlobalUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    uboBuffers[i]->map();
  }

  
  auto globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
                             .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                         VK_SHADER_STAGE_ALL_GRAPHICS)
                             .build();

  std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < static_cast<int>(globalDescriptorSets.size()); i++) {
    auto bufferInfo = uboBuffers[i]->descriptorInfo();
    LveDescriptorWriter(*globalSetLayout, *globalPool)
        .writeBuffer(0, &bufferInfo)
        .build(globalDescriptorSets[i]);
  }

  // Texture set layout (set=1)
  auto textureSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
                              .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                          VK_SHADER_STAGE_FRAGMENT_BIT)
                              .build();

  auto texturePool = LveDescriptorPool::Builder(lveDevice)
                         .setMaxSets(4096)
                         .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096)
                         .build();

  // Tank texture
  auto tankTexture = std::make_shared<LveTexture>(lveDevice, "textures/Tank.png");
  VkDescriptorSet tankTextureSet{VK_NULL_HANDLE};
  VkDescriptorImageInfo tankImageInfo{};
  tankImageInfo.sampler = tankTexture->sampler();
  tankImageInfo.imageView = tankTexture->imageView();
  tankImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  LveDescriptorWriter(*textureSetLayout, *texturePool)
      .writeImage(0, &tankImageInfo)
      .build(tankTextureSet);

  // Missile red texture (1x1)
  auto missileTexture = std::make_shared<LveTexture>(lveDevice, "textures/red.png");
  VkDescriptorSet missileTextureSet{VK_NULL_HANDLE};
  VkDescriptorImageInfo missileImageInfo{};
  missileImageInfo.sampler = missileTexture->sampler();
  missileImageInfo.imageView = missileTexture->imageView();
  missileImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  LveDescriptorWriter(*textureSetLayout, *texturePool)
      .writeImage(0, &missileImageInfo)
      .build(missileTextureSet);

  // City texture
  auto cityTexture = std::make_shared<LveTexture>(lveDevice, "textures/City.jpg");
  VkDescriptorSet cityTextureSet{ VK_NULL_HANDLE };
  VkDescriptorImageInfo cityImageInfo{};
  cityImageInfo.sampler = cityTexture->sampler();
  cityImageInfo.imageView = cityTexture->imageView();
  cityImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  LveDescriptorWriter(*textureSetLayout, *texturePool)
      .writeImage(0, &cityImageInfo)
      .build(cityTextureSet);


  // Ground texture (Marble)
  auto groundTexture = std::make_shared<LveTexture>(lveDevice, "textures/Marble.jpg");
  VkDescriptorSet groundTextureSet{ VK_NULL_HANDLE };
  VkDescriptorImageInfo groundImageInfo{};
  groundImageInfo.sampler = groundTexture->sampler();
  groundImageInfo.imageView = groundTexture->imageView();
  groundImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  LveDescriptorWriter(*textureSetLayout, *texturePool)
      .writeImage(0, &groundImageInfo)
      .build(groundTextureSet);



  // Assign object texture sets
  if (gameObjects.find(tankId) != gameObjects.end()) gameObjects.at(tankId).textureDescriptorSet = tankTextureSet;
  if (gameObjects.find(turretId) != gameObjects.end()) gameObjects.at(turretId).textureDescriptorSet = tankTextureSet;
  if (gameObjects.find(missileId) != gameObjects.end()) gameObjects.at(missileId).textureDescriptorSet = missileTextureSet;
  if (gameObjects.find(townId) != gameObjects.end()) gameObjects.at(townId).textureDescriptorSet = cityTextureSet;
  if (gameObjects.find(groundId) != gameObjects.end()) gameObjects.at(groundId).textureDescriptorSet = groundTextureSet;

  // Town material sets (if supported in your LveModel)
  //if (gameObjects.find(townId) != gameObjects.end() && gameObjects.at(townId).model != nullptr) {
  //  gameObjects.at(townId).model->createMaterialDescriptorSets(*texturePool, *textureSetLayout);
  //}

  SimpleRenderSystem simpleRenderSystem{lveDevice, lveRenderer.getSwapChainRenderPass(),
                                       globalSetLayout->getDescriptorSetLayout(),
                                       textureSetLayout->getDescriptorSetLayout()};
  PointLightSystem pointLightSystem{lveDevice, lveRenderer.getSwapChainRenderPass(),
                                    globalSetLayout->getDescriptorSetLayout()};

  LveCamera camera{};
  auto viewerObject = LveGameObject::createGameObject();
  viewerObject.transform.translation = { 0.f, -4.f, -8.f };

  KeyboardMovementController cameraController{};
  cameraController.moveSpeed = 10.f;   
  cameraController.lookSpeed = 1.5f;   
  
  KeyboardMovementController tankController{};
  tankController.setupForTank();

  auto currentTime = std::chrono::high_resolution_clock::now();

  while (!lveWindow.shouldClose()) {
    glfwPollEvents();

    auto newTime = std::chrono::high_resolution_clock::now();
    float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
    currentTime = newTime;
    frameTime = glm::min(frameTime, 0.1f);

    // camera
    cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
    camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);
    float aspect = lveRenderer.getAspectRatio();
    camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.5f, 500.f);


    // tank + turret + missile
    if (gameObjects.find(tankId) != gameObjects.end() &&
        gameObjects.find(turretId) != gameObjects.end() &&
        gameObjects.find(missileId) != gameObjects.end() ) {  
      auto &tankBody = gameObjects.at(tankId);
      auto &tankTurret = gameObjects.at(turretId);
      auto &missile = gameObjects.at(missileId);
      
      if (glfwJoystickPresent(GLFW_JOYSTICK_1)) {
          tankController.moveTankGamepad(frameTime, tankBody, tankTurret, missile); 
      }
      else {
          tankController.moveTank(lveWindow.getGLFWwindow(), frameTime, tankBody, tankTurret, missile, gameObjects);
      }

    }


    if (auto commandBuffer = lveRenderer.beginFrame()) {
      int frameIndex = lveRenderer.getFrameIndex();
      FrameInfo frameInfo{frameIndex, frameTime, commandBuffer, camera, globalDescriptorSets[frameIndex], gameObjects};

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
  std::shared_ptr<LveModel> lveModel;

  // Tank body
  lveModel = LveModel::createModelFromFile(lveDevice, "models/TankBody.obj");
  auto tankBody = LveGameObject::createGameObject();
  tankBody.model = lveModel;
  tankBody.transform.translation = {8.0f, 1.0f, 20.0f};
  tankBody.transform.scale = {1.0f, -1.0f, 1.0f};
  tankId = tankBody.getId();
  gameObjects.emplace(tankId, std::move(tankBody));

  //  Resimdeki o spesifik binanýn önüne veya içine GÖRÜNMEZ BÝR DUVAR koyalým.
  auto invisibleWall = LveGameObject::createGameObject();
  // Bu koordinatlarý binanýn bulunduðu konuma göre deneme yanýlma ile oturtmalýsýn
  invisibleWall.transform.translation = { 0.0f, 1.0f, 15.0f };
  invisibleWall.transform.scale = { 1.f, 1.f, 1.f };
  invisibleWall.collider = std::make_unique<ColliderComponent>();

  // Kutunun boyutlarý (Örneðin: X ekseninde 10 birim, Z ekseninde 5 birim kalýnlýðýnda bir duvar)
  invisibleWall.collider->minOffset = { -10.0f, -5.0f, -2.5f };
  invisibleWall.collider->maxOffset = { 10.0f, 5.0f, 2.5f };

  gameObjects.emplace(invisibleWall.getId(), std::move(invisibleWall));


  // Turret
  lveModel = LveModel::createModelFromFile(lveDevice, "models/TankTurret+Barrel.obj");
  auto tankTurret = LveGameObject::createGameObject();
  tankTurret.model = lveModel;
  tankTurret.transform.translation = {0.f, 1.0f, 0.f};
  tankTurret.transform.scale = {1.0f, -1.0f, 1.0f};
  turretId = tankTurret.getId();
  gameObjects.emplace(turretId, std::move(tankTurret));

  // Missile (starts attached to muzzle, moved by controller)
  lveModel = LveModel::createModelFromFile(lveDevice, "models/Missile.obj");
  auto missile = LveGameObject::createGameObject();
  missile.model = lveModel;
  //missile.transform.translation = {0.f, -10.f, 0.f};
  missile.transform.scale = {1.0f, -1.0f, 1.0f};
  missileId = missile.getId();
  gameObjects.emplace(missileId, std::move(missile));

  // Town
  lveModel = LveModel::createModelFromFile(lveDevice, "models/City.obj");
  auto town = LveGameObject::createGameObject();
  town.model = lveModel;
  town.transform.translation = {0.f, 1.0f, 0.f};
  town.transform.scale = { 0.5f, -0.5f, 0.5f };
  townId = town.getId();
  gameObjects.emplace(townId, std::move(town));

  // Town kodlarýnýn altýna (Þehri þimdilik büyük tek bir kutu olarak düþünüyoruz):
  town.collider = std::make_unique<ColliderComponent>();
  town.collider->minOffset = { -40.0f, -1.0f, -40.0f };
  town.collider->maxOffset = { 40.0f, 15.0f, 40.0f };

  //Ground
  lveModel = LveModel::createModelFromFile(lveDevice, "models/Ground.obj");
  auto ground = LveGameObject::createGameObject();
  ground.model = lveModel;
  ground.transform.translation = { 0.f, 1.0f, 0.f };
  ground.transform.scale = { 0.5f, -0.5f, 0.5f };
  groundId = ground.getId();
  gameObjects.emplace(groundId, std::move(ground));


}

} // namespace lve
