#include <fstream>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/compatibility.hpp"

#include "renderer.h"

// renderer.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace 
{
    static constexpr ir::RayIntersection MISS = 
    {
        .position = glm::vec3{},
        .normal = glm::vec3{},
        .material = ir::PBRMaterial{},
        .depth = std::numeric_limits<ir::Real>::infinity(),
        .hit = false,
    };
}

namespace ir
{
    RayIntersection Sphere::intersect(const Ray& ray)
    {
        const auto difference = ray.origin - center;

        const auto a = glm::dot(ray.direction, ray.direction);
        const auto b = 2.f * glm::dot(difference, ray.direction);
        const auto c = glm::dot(difference, difference) - (radius * radius);
        const auto d = (b * b) - (4.f * a * c);

        if (d > 0.f)
        {
            const auto t1 = (-b - glm::sqrt(d)) / (2.f * a);
            [[maybe_unused]] const auto t2 = (-b + glm::sqrt(d)) / (2.f * a);

            if (t1 > 0.f)
            {
                const auto via = ray.direction * t1;
                const auto intersection = ray.origin + via;

                const auto reverse = intersection - center;
                const auto normal = glm::normalize(reverse);

                return
                {
                    .position = intersection,
                    .normal = normal,
                    .material = material,
                    .depth = t1,
                    .hit = true,
                    .object = this,
                };
            }
        }

        return MISS;
    }

    glm::vec2 Sphere::compute_uv_coordinates(const glm::vec3& point) const
    {
        // Spherical coordinates https://en.wikipedia.org/wiki/UV_mapping

        const auto p = glm::normalize(point - center);

        const auto u = .5f + glm::atan2(p.z, p.x) / (2.f * glm::pi<Real>());
        const auto v = .5f + glm::asin(p.y) / glm::pi<Real>();

        return { u, v };
    }

    RayIntersection Triangle::intersect(const Ray& ray)
    {
        // Modified Möller-Trumbore from https://en.m.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm

        const auto test = glm::cross(ray.direction, edge1);

        // run Cramer's rule to intersect
        const auto determinant = glm::dot(edge0, test);
        if (glm::abs(determinant) < .001f)
        {
            // parallel
            return MISS;
        }

        const auto inverse_determinant = 1.f / determinant;
        const auto difference = ray.origin - v0;
        const auto u = glm::dot(difference, test) * inverse_determinant;

        if (u < 0.f || u > 1.f)
        {
            // outside
            return MISS;
        }

        const auto q = glm::cross(difference, edge0);
        const auto v = glm::dot(ray.direction, q) * inverse_determinant;

        if (v < 0.f || u + v > 1.f)
        {
            // outside
            return MISS;
        }

        const auto t = glm::dot(edge1, q) * inverse_determinant;
        if (t <= .001f)
        {
            // behind
            return MISS;
        }

        const auto intersection = ray.origin + ray.direction * t;

        return 
        {
            .position = intersection,
            .normal = normal,
            .material = material,
            .depth = t,
            .hit = true,
            .object = this,
        };
    }

    glm::vec2 Triangle::compute_uv_coordinates(const glm::vec3& point) const
    {
        // Barycentric coordinates https://gamedev.stackexchange.com/questions/23743/whats-the-most-efficient-way-to-find-barycentric-coordinates
        // variant of Cramer's rule as used in intersect()

        const auto difference = point - v0;

        const auto d20 = glm::dot(difference, edge0);
        const auto d21 = glm::dot(difference, edge1);

        const auto v = ((d11 * d20) - (d01 * d21)) / denominator;
        const auto w = ((d00 * d21) - (d01 * d20)) / denominator;
        const auto u = 1.f - v - w;

        return u * uv0 + v * uv1 + w * uv2;
    }

    RayIntersection Quadrilateral::intersect(const Ray& ray)
    {
        // quad intersection from https://raytracing.github.io/books/RayTracingTheNextWeek.html
        // finds the plane containing the quad, then intersects the plane and verifies quad boundaries

        const auto denominator = glm::dot(normal, ray.direction);
        if (glm::abs(denominator) < .001f)
        {
            // parallel
            return MISS;
        }

        const auto t = (constant - glm::dot(normal, ray.origin)) / denominator;
        if (t <= .001f)
        {
            // behind
            return MISS;
        }

        const auto intersection = ray.origin + ray.direction * t;

        const auto plane_intersection = intersection - v0;
        const auto alpha = glm::dot(reciprocal, glm::cross(plane_intersection, v1));
        const auto beta = glm::dot(reciprocal, glm::cross(v2, plane_intersection));

        if (alpha < 0.f || alpha > 1.f || beta < 0.f || beta > 1.f)
        {
            // outside
            return MISS;
        }

        return 
        {
            .position = intersection,
            .normal = normal,
            .material = material,
            .depth = t,
            .hit = true,
            .object = this,
        };
    }

    glm::vec2 Quadrilateral::compute_uv_coordinates(const glm::vec3& point) const
    {
        // TODO: UV coordinate computation to be implemented
        return { 0.f, 0.f };
    }

    // (c) Connor J. Link. Partial attribution (meaningful modifications performed herein) from personal work outside of ISU.
    // Utility function that does not meaningfully affect project functionality.
    std::vector<Triangle*> load_obj_as_triangles(const std::string& filepath, const PBRMaterial& material)
    {
        std::vector<Triangle*> triangles{};

        std::ifstream file(filepath);
        if (!file.good())
        {
            return triangles;
        }

        std::vector<glm::vec3> vertices{};

        std::string line;
        while (std::getline(file, line))
        {
            const auto tokens = split(line, " ");
            if (tokens.empty()) 
            {
                continue;
            }

            if (tokens[0] == "v" && tokens.size() >= 4)
            {
                float x = std::stof(tokens[1]);
                float y = std::stof(tokens[2]);
                float z = std::stof(tokens[3]);

                vertices.emplace_back(x, y, z);
            }
            else if (tokens[0] == "f" && tokens.size() >= 4)
            {
                int face0 = std::stoi(tokens[1]);
                int face1 = std::stoi(tokens[2]);
                int face2 = std::stoi(tokens[3]);

                triangles.emplace_back(new Triangle{ vertices[face0 - 1], vertices[face1 - 1], vertices[face2 - 1], glm::vec2{ 0.f, 0.f }, glm::vec2{ 0.f, 1.f }, glm::vec2{ 1.f, 1.f }, material });
            }
        }

        return triangles;
    }
}
