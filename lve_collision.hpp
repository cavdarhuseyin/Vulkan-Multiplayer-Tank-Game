#pragma once

#include "lve_game_object.hpp"
#include <glm/glm.hpp>

namespace lve {

    class LveCollision {
    public:
        // Ýki objenin AABB kutularýnýn kesiþip kesiþmediðini kontrol eder
        static bool checkAABBCollision(
            const glm::vec3& posA, const ColliderComponent& colA, const glm::vec3& scaleA,
            const glm::vec3& posB, const ColliderComponent& colB, const glm::vec3& scaleB);
    };

} // namespace lve