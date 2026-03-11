#include "lve_collision.hpp"
#include <limits>

namespace lve {

    // GLM ile optimize edilmiþ Iþýn-Üçgen kesiþim algoritmasý
    bool LveCollision::rayTriangleIntersect(
        const glm::vec3& orig, const glm::vec3& dir,
        const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
        float& t) {

        const float EPSILON = 0.0000001f;
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 h = glm::cross(dir, edge2);
        float a = glm::dot(edge1, h);

        if (a > -EPSILON && a < EPSILON) return false; // Iþýn üçgene paralel

        float f = 1.0f / a;
        glm::vec3 s = orig - v0;
        float u = f * glm::dot(s, h);
        if (u < 0.0f || u > 1.0f) return false;

        glm::vec3 q = glm::cross(s, edge1);
        float v = f * glm::dot(dir, q);
        if (v < 0.0f || u + v > 1.0f) return false;

        t = f * glm::dot(edge2, q);
        return t > EPSILON; // Çarpýþma kameranýn/tankýn arkasýnda kalmamalý
    }

    // Modelin içindeki TÜM üçgenleri tarayan DX12'deki fonksiyonun birebir iþlevi
    float LveCollision::IntersectModel(
        const glm::vec3& rayOrigin, const glm::vec3& rayDir,
        const LveModel& model, const glm::mat4& modelMatrix) {

        float min_t = std::numeric_limits<float>::max();
        const auto& vertices = model.getVertices();
        const auto& indices = model.getIndices();

        if (model.hasIndices()) {
            for (size_t i = 0; i < indices.size(); i += 3) {
                // Dünya koordinatlarýna çevir (DX12'deki XMVector3Transform karþýlýðý)
                glm::vec3 v0 = modelMatrix * glm::vec4(vertices[indices[i]].position, 1.0f);
                glm::vec3 v1 = modelMatrix * glm::vec4(vertices[indices[i + 1]].position, 1.0f);
                glm::vec3 v2 = modelMatrix * glm::vec4(vertices[indices[i + 2]].position, 1.0f);

                float t = 0.0f;
                if (rayTriangleIntersect(rayOrigin, rayDir, v0, v1, v2, t)) {
                    if (t < min_t) min_t = t;
                }
            }
        }
        return min_t;
    }

} // namespace lve