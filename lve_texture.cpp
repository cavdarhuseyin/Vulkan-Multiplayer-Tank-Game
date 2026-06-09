

#include "lve_texture.hpp"
#include "lve_buffer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdexcept>
#include <cstdint>
#include <string>
#include <cctype>   

namespace lve {

static constexpr VkFormat kTextureFormat = VK_FORMAT_R8G8B8A8_SRGB;

LveTexture::LveTexture(LveDevice& device, const std::string& filepath)
    : lveDevice_{ device } {
    createTextureImage(filepath);
    createTextureImageView();
    createTextureSampler();
}

LveTexture::~LveTexture() {
    if (textureSampler_ != VK_NULL_HANDLE)
        vkDestroySampler(lveDevice_.device(), textureSampler_, nullptr);

    if (textureImageView_ != VK_NULL_HANDLE)
        vkDestroyImageView(lveDevice_.device(), textureImageView_, nullptr);

    if (textureImage_ != VK_NULL_HANDLE)
        vkDestroyImage(lveDevice_.device(), textureImage_, nullptr);

    if (textureImageMemory_ != VK_NULL_HANDLE)
        vkFreeMemory(lveDevice_.device(), textureImageMemory_, nullptr);
}

void LveTexture::createTextureImage(const std::string& filepath) {

    stbi_set_flip_vertically_on_load(true);

    int texWidth = 0, texHeight = 0, texChannels = 0;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);


    if (!pixels) {
        throw std::runtime_error("failed to load texture image: " + filepath);
    }

    width_ = static_cast<uint32_t>(texWidth);
    height_ = static_cast<uint32_t>(texHeight);
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width_) * height_ * 4;

    // Staging buffer (CPU'dan GPU'ya)
    LveBuffer stagingBuffer{
        lveDevice_,
        static_cast<uint32_t>(imageSize), // instanceSize
        1,                                // instanceCount
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer(pixels);
    stbi_image_free(pixels);

    // GPU image oluþturur
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width_;
    imageInfo.extent.height = height_;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = kTextureFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    lveDevice_.createImageWithInfo(
        imageInfo,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        textureImage_,
        textureImageMemory_
    );

    
    lveDevice_.transitionImageLayout(
        textureImage_,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        1  
    );

    
    lveDevice_.copyBufferToImage(
        stagingBuffer.getBuffer(),
        textureImage_,
        width_,
        height_,
        1
    );

    
    lveDevice_.transitionImageLayout(
        textureImage_,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1,
        1
    );
}

void LveTexture::createTextureImageView() {
    textureImageView_ = lveDevice_.createImageView(
        textureImage_,
        kTextureFormat,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1
    );
}

void LveTexture::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = lveDevice_.properties.limits.maxSamplerAnisotropy;

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(lveDevice_.device(), &samplerInfo, nullptr, &textureSampler_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

} 
