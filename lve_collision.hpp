#pragma once

#include "lve_game_object.hpp"
#include <glm/glm.hpp>

namespace lve {

    class LveCollision {
    public:
        // Ray-Triangle Kesiþim Testi (GLM tarafýndan)
        static float IntersectModel(
            const glm::vec3& rayOrigin,
            const glm::vec3& rayDir,
            const LveModel& model,
            const glm::mat4& modelMatrix);

    private:
        static bool rayTriangleIntersect(
            const glm::vec3& orig, const glm::vec3& dir,
            const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
            float& t);
    };

} 