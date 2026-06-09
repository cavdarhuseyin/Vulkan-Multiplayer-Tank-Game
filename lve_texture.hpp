#pragma once

#include "lve_device.hpp"
#include <string>

namespace lve {

	class LveTexture {
	public:
		LveTexture(LveDevice& device, const std::string& filepath);
		~LveTexture();

		LveTexture(const LveTexture&) = delete;
		LveTexture& operator=(const LveTexture&) = delete;

		VkImageView imageView() const { return textureImageView_; }
		VkSampler sampler() const { return textureSampler_; }

		
		VkDescriptorImageInfo descriptorInfo(
			VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const {
			VkDescriptorImageInfo info{};
			info.imageLayout = layout;
			info.imageView = textureImageView_;
			info.sampler = textureSampler_;
			return info;
		}

	private:
		void createTextureImage(const std::string& filepath);
		void createTextureImageView();
		void createTextureSampler();

		LveDevice& lveDevice_;

		VkImage textureImage_{ VK_NULL_HANDLE };
		VkDeviceMemory textureImageMemory_{ VK_NULL_HANDLE };
		VkImageView textureImageView_{ VK_NULL_HANDLE };
		VkSampler textureSampler_{ VK_NULL_HANDLE };

		uint32_t width_{ 0 };
		uint32_t height_{ 0 };
	};

} 
