#include "lve_model.hpp"

#include "lve_descriptors.hpp"
#include "lve_texture.hpp"
#include "lve_utils.hpp"

// libs
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

namespace std {
    template <> struct hash<lve::LveModel::Vertex> {
        size_t operator()(lve::LveModel::Vertex const& vertex) const {
            size_t seed = 0;
            lve::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
            return seed;
        }
    };
} // namespace std

namespace lve {

    static std::string dirnameOf(const std::string& filepath) {
        auto slash = filepath.find_last_of("/\\\\");
        if (slash == std::string::npos) return "";
        return filepath.substr(0, slash);
    }

    static std::string normalizeRelPath(std::string p) {
        // wild_town.mtl uses "/Maps/xxx.jpg" style paths; treat them as relative.
        while (!p.empty() && (p.front() == '/' || p.front() == '\\')) p.erase(p.begin());
        return p;
    }

    LveModel::LveModel(LveDevice& device, const Builder& builder) : lveDevice{ device } {
        createVertexBuffers(builder.vertices);
        createIndexBuffers(builder.indices);
        submeshes_ = builder.submeshes;
        materials_ = builder.materials;

        // Load textures (diffuse only for now)
        for (auto& mat : materials_) {
            if (mat.diffusePath.empty()) continue;
            try {
                mat.diffuseTex = std::make_shared<LveTexture>(lveDevice, mat.diffusePath);
            }
            catch (const std::exception& e) {
                std::cerr << "[LveModel] Failed to load texture: " << mat.diffusePath << " (" << e.what() << ")\n";
                mat.diffuseTex.reset();
            }
        }
    }

    LveModel::~LveModel() {}

    std::unique_ptr<LveModel> LveModel::createModelFromFile(LveDevice& device, const std::string& filepath) {
        Builder builder{};
        builder.loadModel(filepath);
        std::cout << "Vertex count: " << builder.vertices.size() << std::endl;
        std::cout << "Material count: " << builder.materials.size() << std::endl;
        std::cout << "Submesh count: " << builder.submeshes.size() << std::endl;
        return std::make_unique<LveModel>(device, builder);
    }

    void LveModel::createMaterialDescriptorSets(LveDescriptorPool& pool, LveDescriptorSetLayout& textureSetLayout) {
        for (auto& mat : materials_) {
            if (!mat.diffuseTex) {
                mat.descriptorSet = VK_NULL_HANDLE;
                continue;
            }

            VkDescriptorImageInfo imageInfo = mat.diffuseTex->descriptorInfo();
            LveDescriptorWriter(textureSetLayout, pool)
                .writeImage(0, &imageInfo)
                .build(mat.descriptorSet);
        }
    }

    VkDescriptorSet LveModel::getMaterialDescriptorSet(int materialIndex) const {
        if (materialIndex < 0 || materialIndex >= static_cast<int>(materials_.size())) {
            return VK_NULL_HANDLE;
        }
        return materials_[materialIndex].descriptorSet;
    }

    void LveModel::createVertexBuffers(const std::vector<Vertex>& vertices) {
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3");

        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
        uint32_t vertexSize = sizeof(vertices[0]);

        LveBuffer stagingBuffer{
            lveDevice,
            vertexSize,
            vertexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*)vertices.data());

        vertexBuffer = std::make_unique<LveBuffer>(
            lveDevice,
            vertexSize,
            vertexCount,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        lveDevice.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
    }

    void LveModel::createIndexBuffers(const std::vector<uint32_t>& indices) {
        indexCount = static_cast<uint32_t>(indices.size());
        hasIndexBuffer = indexCount > 0;
        if (!hasIndexBuffer) return;

        VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
        uint32_t indexSize = sizeof(indices[0]);

        LveBuffer stagingBuffer{
            lveDevice,
            indexSize,
            indexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*)indices.data());

        indexBuffer = std::make_unique<LveBuffer>(
            lveDevice,
            indexSize,
            indexCount,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        lveDevice.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
    }

    void LveModel::bind(VkCommandBuffer commandBuffer) {
        VkBuffer buffers[] = { vertexBuffer->getBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

        if (hasIndexBuffer) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
    }

    void LveModel::draw(VkCommandBuffer commandBuffer) {
        if (hasIndexBuffer) {
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        }
        else {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }

    void LveModel::drawSubmesh(VkCommandBuffer commandBuffer, const Submesh& submesh) {
        if (!hasIndexBuffer) {
            draw(commandBuffer);
            return;
        }
        vkCmdDrawIndexed(commandBuffer, submesh.indexCount, 1, submesh.firstIndex, 0, 0);
    }

    std::vector<VkVertexInputBindingDescription> LveModel::Vertex::getBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(Vertex);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescriptions;
    }

    std::vector<VkVertexInputAttributeDescription> LveModel::Vertex::getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) });
        attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color) });
        attributeDescriptions.push_back({ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) });
        attributeDescriptions.push_back({ 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) });
        return attributeDescriptions;
    }

    void LveModel::Builder::loadModel(const std::string& filepath) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> tinyMats;
        std::string warn, err;

        const std::string baseDir = dirnameOf(filepath);

        // SADECE wild_town için V eksenini flip edeceðiz
        const bool flipVForThisModel =
            (filepath.find("wild_town") != std::string::npos);

        if (!tinyobj::LoadObj(&attrib, &shapes, &tinyMats, &warn, &err, filepath.c_str(), baseDir.c_str())) {
            throw std::runtime_error(warn + err);
        }

        if (!warn.empty()) {
            std::cerr << "[tinyobj] " << warn << "\n";
        }

        vertices.clear();
        indices.clear();
        submeshes.clear();
        materials.clear();

        // --- Materials prepare ---
        if (tinyMats.empty()) {
            Material mat{};
            mat.diffusePath = "";
            materials.push_back(mat);
        }
        else {
            materials.resize(tinyMats.size());
            for (size_t i = 0; i < tinyMats.size(); i++) {
                const auto& m = tinyMats[i];
                Material mat{};
                if (!m.diffuse_texname.empty()) {
                    const std::string rel = normalizeRelPath(m.diffuse_texname);
                    mat.diffusePath = baseDir.empty() ? rel : (baseDir + "/" + rel);
                }
                materials[i] = mat;
            }
        }

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        // indices grouped per material
        std::vector<std::vector<uint32_t>> perMatIndices(materials.size());

        for (const auto& shape : shapes) {
            size_t indexOffset = 0;
            const auto& mesh = shape.mesh;

            for (size_t face = 0; face < mesh.num_face_vertices.size(); face++) {
                const int fv = mesh.num_face_vertices[face];
                const int matId = (face < mesh.material_ids.size() ? mesh.material_ids[face] : -1);
                const int safeMat = (matId >= 0 && matId < static_cast<int>(perMatIndices.size())) ? matId : 0;

                for (int v = 0; v < fv; v++) {
                    const tinyobj::index_t idx = mesh.indices[indexOffset + v];

                    Vertex vertex{};

                    // position + color
                    if (idx.vertex_index >= 0) {
                        vertex.position = {
                            attrib.vertices[3 * idx.vertex_index + 0],
                            attrib.vertices[3 * idx.vertex_index + 1],
                            attrib.vertices[3 * idx.vertex_index + 2],
                        };

                        if (!attrib.colors.empty()) {
                            vertex.color = {
                                attrib.colors[3 * idx.vertex_index + 0],
                                attrib.colors[3 * idx.vertex_index + 1],
                                attrib.colors[3 * idx.vertex_index + 2],
                            };
                        }
                        else {
                            vertex.color = { 1.f, 1.f, 1.f };
                        }
                    }

                    // normal
                    if (idx.normal_index >= 0 && !attrib.normals.empty()) {
                        vertex.normal = {
                            attrib.normals[3 * idx.normal_index + 0],
                            attrib.normals[3 * idx.normal_index + 1],
                            attrib.normals[3 * idx.normal_index + 2],
                        };
                    }

                    //  UV (wild_town için sadece V flip)
                    if (idx.texcoord_index >= 0 && !attrib.texcoords.empty()) {
                        float u = attrib.texcoords[2 * idx.texcoord_index + 0];
                        float vv = attrib.texcoords[2 * idx.texcoord_index + 1];

                        if (flipVForThisModel) {
                            vv = 1.0f - vv;
                        }

                        vertex.uv = { u, vv };
                    }

                    if (uniqueVertices.count(vertex) == 0) {
                        uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                        vertices.push_back(vertex);
                    }

                    perMatIndices[safeMat].push_back(uniqueVertices[vertex]);
                }

                indexOffset += fv;
            }
        }

        // --- Build one index buffer + submeshes ---
        for (int mat = 0; mat < static_cast<int>(perMatIndices.size()); mat++) {
            auto& vec = perMatIndices[mat];
            if (vec.empty()) continue;

            Submesh sm{};
            sm.firstIndex = static_cast<uint32_t>(indices.size());
            sm.indexCount = static_cast<uint32_t>(vec.size());
            sm.materialIndex = mat;

            submeshes.push_back(sm);
            indices.insert(indices.end(), vec.begin(), vec.end());
        }
    }

} // namespace lve
