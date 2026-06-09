#pragma once


#include "lve_model.hpp"

#include <glm/gtc/matrix_transform.hpp>


#include <memory>
#include <unordered_map>


namespace lve {


	struct TransformComponent {
		
		glm::vec3 translation{}; // taþýma vektörü, baþlangýçta sýfýr vektör
		glm::vec3 scale{ 1.f, 1.f , 1.f }; // ölçeklendirme vektörü, baþlangýçta birim vektör
		glm::vec3 rotation{}; // döndürme vektörü, baþlangýçta sýfýr vektör

		// Matris hesaplama fonksiyonlarý
	   // Rotation sýrasýný YXZ olarak belirledik, bu sýrayla döndürme iþlemi yapýlacak
		glm::mat4 mat4();

		glm::mat3 normalMatrix();
	};

	struct PointLightComponent {

		float lightIntensity = 1.0f;
	};

	class LveGameObject {

	  public:
		  using id_t = unsigned int;
		  using Map = std::unordered_map<id_t, LveGameObject>;

		  bool isActive = true; //Obje varsayýlan olarak aktiftir

		  static LveGameObject createGameObject() {

             static id_t currentId = 0;
			 return LveGameObject{ currentId++ };

		  }

		  static LveGameObject makePointLight(
			  float intensity = 10.f, float radius = 0.1f, glm::vec3 color = glm::vec3(1.f));

		  LveGameObject(const LveGameObject&) = delete;
		  LveGameObject& operator=(const LveGameObject&) = delete;
		  LveGameObject(LveGameObject&&) = default;
		  LveGameObject& operator=(LveGameObject&&) = default;


		  id_t getId() { return id; }

		  glm::vec3 color{};
		  TransformComponent transform{};
		  

		  
		  std::shared_ptr<LveModel> model{};

		  // Texture seti, materyal için gerekli olabilir
		  VkDescriptorSet textureDescriptorSet{VK_NULL_HANDLE};
		  std::unique_ptr<PointLightComponent> pointLight = nullptr;

      private:
		  LveGameObject(id_t objId) : id{objId} {}
		  
		  id_t id;

	};




}
