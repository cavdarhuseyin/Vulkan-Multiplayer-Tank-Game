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

	// Submesh struct'ý, modelin farklý parçalarýný temsil eder ve her parça için index aralýðý ve materyal bilgisi tutar
  struct Submesh {
    uint32_t firstIndex{0};
    uint32_t indexCount{0};
    int materialIndex{0};
  };

  // Material struct'ý, her materyal için diffuse texture bilgisi ve descriptor set'ini tutar
  struct Material {
	  std::string diffusePath;                 // texture dosya yolu (yükleme sýrasýnda kullanýlýr)
	  std::shared_ptr<LveTexture> diffuseTex;  // Yüklenen texture nesnesi
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

  // Materyal descriptor setlerini oluþturacak fonksiyon (texture set layout'ý parametre olarak alýr)
  void createMaterialDescriptorSets(LveDescriptorPool &pool, LveDescriptorSetLayout &textureSetLayout);

  const std::vector<Submesh> &getSubmeshes() const { return submeshes_; }
  VkDescriptorSet getMaterialDescriptorSet(int materialIndex) const;

  void bind(VkCommandBuffer commandBuffer);
  void draw(VkCommandBuffer commandBuffer);
  void drawSubmesh(VkCommandBuffer commandBuffer, const Submesh &submesh);



private:
  void createVertexBuffers(const std::vector<Vertex> &vertices);
  void createIndexBuffers(const std::vector<uint32_t> &indices);

  // Model verilerini CPU'da tutacak deðiþkenler
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

} 
