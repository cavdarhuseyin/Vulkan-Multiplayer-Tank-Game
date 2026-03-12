#pragma once

#include "lve_buffer.hpp"
#include "lve_device.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

namespace lve {

class LveDescriptorPool;
class LveDescriptorSetLayout;
class LveTexture;

class LveModel {
public:
  struct Vertex {
    glm::vec3 position{};
    glm::vec3 color{};
    glm::vec3 normal{};
    glm::vec2 uv{};

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

    bool operator==(const Vertex &other) const {
      return position == other.position && color == other.color && normal == other.normal && uv == other.uv;
    }
  };

    // Getter fonksiyonlarýmýz (çarpýþma testi için dýþarýdan eriþeceðiz)
    const std::vector<Vertex>& getVertices() const { return vertices_cpu; }
    const std::vector<uint32_t>& getIndices() const { return indices_cpu; }
    bool hasIndices() const { return hasIndexBuffer; }

  // One draw call region in the shared index buffer
  struct Submesh {
    uint32_t firstIndex{0};
    uint32_t indexCount{0};
    int materialIndex{0};
  };

  // Simple material (only diffuse/albedo texture for now)
  struct Material {
    std::string diffusePath;                 // resolved path on disk
    std::shared_ptr<LveTexture> diffuseTex;  // loaded texture (can be null)
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
  };

  struct Builder {
    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};
    std::vector<Submesh> submeshes{};
    std::vector<Material> materials{};

    void loadModel(const std::string &filepath);
  };

  LveModel(LveDevice &device, const Builder &builder);
  ~LveModel();

  LveModel(const LveModel &) = delete;
  LveModel &operator=(const LveModel &) = delete;

  static std::unique_ptr<LveModel> createModelFromFile(LveDevice &device, const std::string &filepath);

  // Create descriptor sets for each material (set=1 binding=0 sampler2D)
  void createMaterialDescriptorSets(LveDescriptorPool &pool, LveDescriptorSetLayout &textureSetLayout);

  const std::vector<Submesh> &getSubmeshes() const { return submeshes_; }
  VkDescriptorSet getMaterialDescriptorSet(int materialIndex) const;

  void bind(VkCommandBuffer commandBuffer);
  void draw(VkCommandBuffer commandBuffer);
  void drawSubmesh(VkCommandBuffer commandBuffer, const Submesh &submesh);



private:
  void createVertexBuffers(const std::vector<Vertex> &vertices);
  void createIndexBuffers(const std::vector<uint32_t> &indices);

  // Model verilerini CPU'da tutacak deðiþkenlerimiz
  std::vector<Vertex> vertices_cpu;
  std::vector<uint32_t> indices_cpu;

  LveDevice &lveDevice;

  std::unique_ptr<LveBuffer> vertexBuffer{};
  uint32_t vertexCount{0};

  bool hasIndexBuffer{false};
  std::unique_ptr<LveBuffer> indexBuffer{};
  uint32_t indexCount{0};

  std::vector<Submesh> submeshes_{};
  std::vector<Material> materials_{};



};

} // namespace lve
