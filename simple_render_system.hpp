#pragma once

#include "lve_device.hpp"
#include "lve_frame_info.hpp"
#include "lve_pipeline.hpp"

#include <memory>

namespace lve {

class SimpleRenderSystem {
public:
  // set=0: global (UBO etc.)
  // set=1: per-material/object texture
  SimpleRenderSystem(
      LveDevice &device,
      VkRenderPass renderPass,
      VkDescriptorSetLayout globalSetLayout,
      VkDescriptorSetLayout textureSetLayout);

  ~SimpleRenderSystem();

  SimpleRenderSystem(const SimpleRenderSystem &) = delete;
  SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;

  void renderGameObjects(FrameInfo &frameInfo);

private:
  void createPipelineLayout(
      VkDescriptorSetLayout globalSetLayout,
      VkDescriptorSetLayout textureSetLayout);

  void createPipeline(VkRenderPass renderPass);

  LveDevice &lveDevice;
  std::unique_ptr<LvePipeline> lvePipeline;
  VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
};

} // namespace lve
