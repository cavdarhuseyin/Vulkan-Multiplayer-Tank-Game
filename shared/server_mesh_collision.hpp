#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace lve {
namespace net {

class ServerMeshCollision {
public:
    struct Vec3 {
        float x{0.f};
        float y{0.f};
        float z{0.f};
    };

    struct Triangle {
        Vec3 v0{};
        Vec3 v1{};
        Vec3 v2{};
    };

    bool loadObj(
        const std::string& filepath,
        const Vec3& translation,
        const Vec3& scale);

    bool raycast(
        const Vec3& origin,
        const Vec3& direction,
        float maxDistance,
        float& outDistance) const;

    const std::vector<Triangle>& triangles() const { return triangles_; }
    bool empty() const { return triangles_.empty(); }

private:
    static bool parseFaceVertexIndex(const std::string& token, int& outIndex);
    static bool rayTriangleIntersect(
        const Vec3& orig,
        const Vec3& dir,
        const Triangle& tri,
        float& t);

    std::vector<Triangle> triangles_{};
};

} // namespace net
} // namespace lve
