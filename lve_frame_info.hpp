#pragma once

#include "lve_camera.hpp"
#include "lve_game_object.hpp"

// lib
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace lve {

#define MAX_LIGHTS 10

	struct PointLight {
		glm::vec4 position{}; 
		glm::vec4 color{};    // w yoðunluk (intensity)
	};

	struct DirectionalLight {
		glm::vec4 direction{}; // xyz = doðrultu (normalize edilmiþ)
		glm::vec4 color{};     // rgb = renk, w = yoðunluk (intensity)
	};

	struct GlobalUbo {
		glm::mat4 projection{ 1.f };
		glm::mat4 view{ 1.f };
		glm::mat4 inverseView{ 1.f };

		glm::vec4 ambientLightColor{ 1.f, 1.f, 1.f, 0.25f }; 

		PointLight pointLights[MAX_LIGHTS];

		// alingnment için padding (numLights eðer pointlight varsa)
		int numLights{ 0 };
		glm::vec3 _pad0{ 0.f };

		DirectionalLight sunLight{};
	};

	struct FrameInfo {
		int frameIndex;
		float frameTime;
		VkCommandBuffer commandBuffer;
		LveCamera& camera;
		VkDescriptorSet globalDescriptorSet;
		LveGameObject::Map& gameObjects;
	};

} // namespace lve
