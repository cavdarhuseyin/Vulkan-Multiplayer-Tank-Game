#include "simple_render_system.hpp"

#include "lve_model.hpp"

// libraries
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <stdexcept>
#include <vector>

namespace lve {

struct alignas(16) SimplePushConstantData {
  glm::mat4 modelMatrix{1.f};
  glm::mat4 normalMatrix{1.f};
};

SimpleRenderSystem::SimpleRenderSystem(
    LveDevice &device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout globalSetLayout,
    VkDescriptorSetLayout textureSetLayout)
    : lveDevice{device} {
  createPipelineLayout(globalSetLayout, textureSetLayout);
  createPipeline(renderPass);
}

SimpleRenderSystem::~SimpleRenderSystem() {
  if (pipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
  }
}

void SimpleRenderSystem::createPipelineLayout(
    VkDescriptorSetLayout globalSetLayout,
    VkDescriptorSetLayout textureSetLayout) {
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(SimplePushConstantData);

  // set=0 global, set=1 texture
  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout, textureSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }
}

void SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
  if (pipelineLayout == VK_NULL_HANDLE) {
    throw std::runtime_error("Cannot create pipeline before pipeline layout");
  }

  PipelineConfigInfo pipelineConfig{};
  LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.renderPass = renderPass;
  pipelineConfig.pipelineLayout = pipelineLayout;

  // dynamic viewport/scissor
  pipelineConfig.viewportInfo.pViewports = nullptr;
  pipelineConfig.viewportInfo.viewportCount = 1;
  pipelineConfig.viewportInfo.pScissors = nullptr;
  pipelineConfig.viewportInfo.scissorCount = 1;

  lvePipeline = std::make_unique<LvePipeline>(
      lveDevice,
      "shaders/simple_shader.vert.spv",
      "shaders/simple_shader.frag.spv",
      pipelineConfig);
}

void SimpleRenderSystem::renderGameObjects(FrameInfo &frameInfo) {
  lvePipeline->bind(frameInfo.commandBuffer);

  // Bind global set (set=0)
  vkCmdBindDescriptorSets(
      frameInfo.commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      0,
      1,
      &frameInfo.globalDescriptorSet,
      0,
      nullptr);

  for (auto &kv : frameInfo.gameObjects) {
    auto &obj = kv.second;

    // Obje aktif deðilse veya modeli yoksa atla (render etme)
    if (!obj.isActive || obj.model == nullptr) continue;

    SimplePushConstantData push{};
    push.modelMatrix = obj.transform.mat4();
    push.normalMatrix = obj.transform.normalMatrix();

    vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(SimplePushConstantData),
        &push);

    obj.model->bind(frameInfo.commandBuffer);


    const auto &submeshes = obj.model->getSubmeshes();
    if (!submeshes.empty()) {
      for (const auto &sm : submeshes) {
        VkDescriptorSet texSet = obj.model->getMaterialDescriptorSet(sm.materialIndex);
        if (texSet == VK_NULL_HANDLE) texSet = obj.textureDescriptorSet;

        if (texSet != VK_NULL_HANDLE) {
          vkCmdBindDescriptorSets(
              frameInfo.commandBuffer,
              VK_PIPELINE_BIND_POINT_GRAPHICS,
              pipelineLayout,
              1, // set=1
              1,
              &texSet,
              0,
              nullptr);
        }

        obj.model->drawSubmesh(frameInfo.commandBuffer, sm);
      }
    } else {
      // single draw
      if (obj.textureDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            1,
            1,
            &obj.textureDescriptorSet,
            0,
            nullptr);
      }
      obj.model->draw(frameInfo.commandBuffer);
    }
  }
}

} 
