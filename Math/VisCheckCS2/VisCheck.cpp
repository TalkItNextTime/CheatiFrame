#include "VisCheck.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>
#include <future>

const size_t LEAF_THRESHOLD = 4;

VisCheck::VisCheck(const std::string& optimizedGeometryFile) {
    if (!geometry.LoadFromFile(optimizedGeometryFile)) {
        std::cerr << "Failed to load optimized file: " << optimizedGeometryFile << std::endl;
        ready = false;
        return;
    }

    std::vector<std::future<std::unique_ptr<BVHNode>>> buildTasks{};
    buildTasks.reserve(geometry.meshes.size());

    for (const auto& mesh : geometry.meshes)
    {
        buildTasks.push_back(std::async(std::launch::async, [this, &mesh]() {
            return BuildBVH(mesh);
        }));
    }

    for (auto& task : buildTasks)
    {
        bvhNodes.push_back(task.get());
    }

    ready = !bvhNodes.empty();
}

bool VisCheck::IsReady() const
{
    return ready;
}

std::unique_ptr<BVHNode> VisCheck::BuildBVH(const std::vector<TriangleCombined>& tris) {
    auto node = std::make_unique<BVHNode>();

    if (tris.empty()) return node;
    AABB bounds = tris[0].ComputeAABB();
    for (size_t i = 1; i < tris.size(); ++i) {
        AABB triAABB = tris[i].ComputeAABB();
        bounds.min.x = std::min(bounds.min.x, triAABB.min.x);
        bounds.min.y = std::min(bounds.min.y, triAABB.min.y);
        bounds.min.z = std::min(bounds.min.z, triAABB.min.z);
        bounds.max.x = std::max(bounds.max.x, triAABB.max.x);
        bounds.max.y = std::max(bounds.max.y, triAABB.max.y);
        bounds.max.z = std::max(bounds.max.z, triAABB.max.z);
    }
    node->bounds = bounds;
    if (tris.size() <= LEAF_THRESHOLD) {
        node->triangles = tris;
        return node;
    }
    Vector3 diff = VectorSub(bounds.max, bounds.min);
    int axis = (diff.x > diff.y && diff.x > diff.z) ? 0 : ((diff.y > diff.z) ? 1 : 2);
    std::vector<TriangleCombined> sortedTris = tris;
    std::sort(sortedTris.begin(), sortedTris.end(), [axis](const TriangleCombined& a, const TriangleCombined& b) {
        AABB aabbA = a.ComputeAABB();
        AABB aabbB = b.ComputeAABB();
        float centerA, centerB;
        if (axis == 0) {
            centerA = (aabbA.min.x + aabbA.max.x) / 2.0f;
            centerB = (aabbB.min.x + aabbB.max.x) / 2.0f;
        }
        else if (axis == 1) {
            centerA = (aabbA.min.y + aabbA.max.y) / 2.0f;
            centerB = (aabbB.min.y + aabbB.max.y) / 2.0f;
        }
        else {
            centerA = (aabbA.min.z + aabbA.max.z) / 2.0f;
            centerB = (aabbB.min.z + aabbB.max.z) / 2.0f;
        }
        return centerA < centerB;
        });

    size_t mid = sortedTris.size() / 2;
    std::vector<TriangleCombined> leftTris(sortedTris.begin(), sortedTris.begin() + mid);
    std::vector<TriangleCombined> rightTris(sortedTris.begin() + mid, sortedTris.end());

    node->left = BuildBVH(leftTris);
    node->right = BuildBVH(rightTris);

    return node;
}

bool VisCheck::IntersectBVH(const BVHNode* node, const Vector3& rayOrigin, const Vector3& rayDir, float maxDistance, float& hitDistance) {
    if (!node)
        return false;

    if (!node->bounds.RayIntersects(rayOrigin, rayDir)) {
        return false;
    }

    bool hit = false;
    if (node->IsLeaf()) {
        for (const auto& tri : node->triangles) {
            float t;
            if (RayIntersectsTriangle(rayOrigin, rayDir, tri, t)) {
                if (t < maxDistance && t < hitDistance) {
                    hitDistance = t;
                    hit = true;
                }
            }
        }
    }
    else {
        if (node->left) {
            hit |= IntersectBVH(node->left.get(), rayOrigin, rayDir, maxDistance, hitDistance);
        }
        if (node->right) {
            hit |= IntersectBVH(node->right.get(), rayOrigin, rayDir, maxDistance, hitDistance);
        }
    }
    return hit;
}

bool VisCheck::IsPointVisible(const Vector3& point1, const Vector3& point2)
{
    if (!ready)
        return true;

    Vector3 rayDir = { point2.x - point1.x, point2.y - point1.y, point2.z - point1.z };
    float distance = std::sqrt(VectorDot(rayDir, rayDir));
    if (distance <= 1e-4f)
        return true;

    rayDir = { rayDir.x / distance, rayDir.y / distance, rayDir.z / distance };

    float hitDistance = std::numeric_limits<float>::max();
    for (const auto& bvhRoot : bvhNodes) {
        if (!bvhRoot)
            continue;

        if (IntersectBVH(bvhRoot.get(), point1, rayDir, distance, hitDistance)) {
            if (hitDistance < distance) {
                return false;
            }
        }
    }
    return true;
}

bool VisCheck::RayIntersectsTriangle(const Vector3& rayOrigin, const Vector3& rayDir, const TriangleCombined& triangle, float& t)
{
    const float EPSILON = 1e-7f;

    Vector3 edge1 = VectorSub(triangle.v1, triangle.v0);
    Vector3 edge2 = VectorSub(triangle.v2, triangle.v0);
    Vector3 h = VectorCross(rayDir, edge2);
    float a = VectorDot(edge1, h);

    if (a > -EPSILON && a < EPSILON)
        return false;

    float f = 1.0f / a;
    Vector3 s = VectorSub(rayOrigin, triangle.v0);
    float u = f * VectorDot(s, h);

    if (u < 0.0f || u > 1.0f)
        return false;

    Vector3 q = VectorCross(s, edge1);
    float v = f * VectorDot(rayDir, q);

    if (v < 0.0f || u + v > 1.0f)
        return false;

    t = f * VectorDot(edge2, q);

    return (t > EPSILON);
}
