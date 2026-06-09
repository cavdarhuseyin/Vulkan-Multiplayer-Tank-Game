#include "server_mesh_collision.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace lve {
namespace net {

namespace {
ServerMeshCollision::Vec3 operator+(const ServerMeshCollision::Vec3& a, const ServerMeshCollision::Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

ServerMeshCollision::Vec3 operator-(const ServerMeshCollision::Vec3& a, const ServerMeshCollision::Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

ServerMeshCollision::Vec3 operator*(const ServerMeshCollision::Vec3& a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}

float dot(const ServerMeshCollision::Vec3& a, const ServerMeshCollision::Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

ServerMeshCollision::Vec3 cross(const ServerMeshCollision::Vec3& a, const ServerMeshCollision::Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float lengthSq(const ServerMeshCollision::Vec3& v) {
    return dot(v, v);
}

ServerMeshCollision::Vec3 normalize(const ServerMeshCollision::Vec3& v) {
    const float len2 = lengthSq(v);
    if (len2 <= 1e-12f) return {0.f, 0.f, 0.f};
    const float inv = 1.0f / std::sqrt(len2);
    return v * inv;
}
}

bool ServerMeshCollision::parseFaceVertexIndex(const std::string& token, int& outIndex) {
    if (token.empty()) return false;
    std::size_t slashPos = token.find('/');
    std::string indexPart = token.substr(0, slashPos);
    if (indexPart.empty()) return false;
    outIndex = std::stoi(indexPart);
    return true;
}

bool ServerMeshCollision::loadObj(
    const std::string& filepath,
    const Vec3& translation,
    const Vec3& scale) {

    triangles_.clear();

    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    std::vector<Vec3> vertices;
    std::string line;

    while (std::getline(file, line)) {
        if (line.size() < 2) continue;

        if (line[0] == 'v' && std::isspace(static_cast<unsigned char>(line[1]))) {
            std::istringstream iss(line.substr(2));
            Vec3 v{};
            iss >> v.x >> v.y >> v.z;
            v.x = v.x * scale.x + translation.x;
            v.y = v.y * scale.y + translation.y;
            v.z = v.z * scale.z + translation.z;
            vertices.push_back(v);
        }
        else if (line[0] == 'f' && std::isspace(static_cast<unsigned char>(line[1]))) {
            std::istringstream iss(line.substr(2));
            std::vector<int> faceIndices;
            std::string token;

            while (iss >> token) {
                int idx = 0;
                if (parseFaceVertexIndex(token, idx)) {
                    if (idx < 0) {
                        idx = static_cast<int>(vertices.size()) + idx + 1;
                    }
                    faceIndices.push_back(idx - 1);
                }
            }

            if (faceIndices.size() < 3) continue;

            for (std::size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                const int i0 = faceIndices[0];
                const int i1 = faceIndices[i];
                const int i2 = faceIndices[i + 1];

                if (i0 < 0 || i1 < 0 || i2 < 0 ||
                    i0 >= static_cast<int>(vertices.size()) ||
                    i1 >= static_cast<int>(vertices.size()) ||
                    i2 >= static_cast<int>(vertices.size())) {
                    continue;
                }

                triangles_.push_back({ vertices[i0], vertices[i1], vertices[i2] });
            }
        }
    }

    return !triangles_.empty();
}

bool ServerMeshCollision::rayTriangleIntersect(
    const Vec3& orig,
    const Vec3& dir,
    const Triangle& tri,
    float& t) {

    constexpr float EPSILON = 1e-7f;

    const Vec3 edge1 = tri.v1 - tri.v0;
    const Vec3 edge2 = tri.v2 - tri.v0;
    const Vec3 h = cross(dir, edge2);
    const float a = dot(edge1, h);

    if (a > -EPSILON && a < EPSILON) return false;

    const float f = 1.0f / a;
    const Vec3 s = orig - tri.v0;
    const float u = f * dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    const Vec3 q = cross(s, edge1);
    const float v = f * dot(dir, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    t = f * dot(edge2, q);
    return t > EPSILON;
}

bool ServerMeshCollision::raycast(
    const Vec3& origin,
    const Vec3& direction,
    float maxDistance,
    float& outDistance) const {

    if (triangles_.empty()) {
        outDistance = std::numeric_limits<float>::max();
        return false;
    }

    const Vec3 dir = normalize(direction);
    if (lengthSq(dir) <= 1e-12f) {
        outDistance = std::numeric_limits<float>::max();
        return false;
    }

    float closest = std::numeric_limits<float>::max();
    bool hit = false;

    for (const auto& tri : triangles_) {
        float t = 0.f;
        if (rayTriangleIntersect(origin, dir, tri, t)) {
            if (t < closest) {
                closest = t;
                hit = true;
            }
        }
    }

    outDistance = closest;
    return hit && closest <= maxDistance;
}

} 
} 
