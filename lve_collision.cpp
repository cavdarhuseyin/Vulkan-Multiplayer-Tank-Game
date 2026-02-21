#include "lve_collision.hpp"

namespace lve {

    bool LveCollision::checkAABBCollision(
        const glm::vec3& posA, const ColliderComponent& colA, const glm::vec3& scaleA,
        const glm::vec3& posB, const ColliderComponent& colB, const glm::vec3& scaleB) {

        // Negatif scale ihtimaline karþý gerçek min ve max noktalarýný garantiliyoruz
        glm::vec3 aP1 = posA + (colA.minOffset * scaleA);
        glm::vec3 aP2 = posA + (colA.maxOffset * scaleA);
        glm::vec3 actualMinA = glm::min(aP1, aP2);
        glm::vec3 actualMaxA = glm::max(aP1, aP2);

        glm::vec3 bP1 = posB + (colB.minOffset * scaleB);
        glm::vec3 bP2 = posB + (colB.maxOffset * scaleB);
        glm::vec3 actualMinB = glm::min(bP1, bP2);
        glm::vec3 actualMaxB = glm::max(bP1, bP2);

        // Kesiþim Testi
        bool collisionX = actualMaxA.x >= actualMinB.x && actualMinA.x <= actualMaxB.x;
        bool collisionY = actualMaxA.y >= actualMinB.y && actualMinA.y <= actualMaxB.y;
        bool collisionZ = actualMaxA.z >= actualMinB.z && actualMinA.z <= actualMaxB.z;

        return collisionX && collisionY && collisionZ;
    }

} // namespace lve