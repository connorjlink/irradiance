#include <fstream>
#include <cctype>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/compatibility.hpp"
#include "glm/gtc/random.hpp"

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
                const auto intersection = ray.origin + ray.direction * t1;

                const auto reverse = intersection - center;
                const auto normal = glm::normalize(reverse);

                // Spherical coordinates https://en.wikipedia.org/wiki/UV_mapping
                const auto p = glm::normalize(intersection - center);

                const auto u = .5f + glm::atan2(p.z, p.x) / (2.f * glm::pi<Real>());
                const auto v = .5f + glm::asin(p.y) / glm::pi<Real>();

                return
                {
                    .position = intersection,
                    .normal = normal,
                    .material = material,
                    .depth = t1,
                    .exit = t2,
                    .hit = true,
                    .object = this,
                    .uv = { u, v },
                };
            }
        }

        return MISS;
    }

    glm::vec3 Sphere::sample()
    {
        return center + glm::sphericalRand(radius);
    }

    glm::vec3 Sphere::normal_of(const glm::vec3& position)
    {
        return glm::normalize(position - center);
    }

    RayIntersection Triangle::intersect(const Ray& ray)
    {
        // Modified Möller-Trumbore from https://en.m.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm

        const auto test = glm::cross(ray.direction, edge1);

        // run Cramer's rule to intersect and get barycentric coordinates as UV
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
            .uv = { u, v },
        };
    }

    glm::vec3 Triangle::sample()
    {
        // compute as uniform barycentric coordinates, modified from 
        // https://stackoverflow.com/questions/4778147/sample-random-point-in-triangle
        const auto sqrt_r1 = glm::sqrt(glm::linearRand(0.f, 1.f));
        const auto r2 = glm::linearRand(0.f, 1.f);

        const auto u = 1.f - sqrt_r1;
        const auto v = r2 * sqrt_r1;

        return (1.f - u - v) * v0 + u * v1 + v * v2;
    }

    glm::vec3 Triangle::normal_of(const glm::vec3& position)
    {
        return normal;
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
        // equivalent to the alpha, beta products in RTTNW quad algorithm
        const auto u = glm::dot(reciprocal, glm::cross(plane_intersection, v1));
        const auto v = glm::dot(reciprocal, glm::cross(v2, plane_intersection));

        if (u < 0.f || u > 1.f || v < 0.f || v > 1.f)
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
            .uv = { u, v },
        };
    }

    glm::vec3 Quadrilateral::sample()
    {
        // simple offsets into the parallelogram
        const auto u = glm::linearRand(0.f, 1.f);
        const auto v = glm::linearRand(0.f, 1.f);

        return v0 + u * v1 + v * v2;
    }

    glm::vec3 Quadrilateral::normal_of(const glm::vec3& position)
    {
        return normal;
    }

    RayIntersection Colloid::intersect(const Ray& ray)
    {
        const auto intersection = container->intersect(ray);
        if (!intersection.hit)
        {
            return MISS;
        }

        const auto entry = ray.origin + ray.direction * intersection.depth;
        const auto exit = ray.origin + ray.direction * intersection.exit;
        const auto scatter_distance = glm::length(exit - entry);
        if (scatter_distance <= 0.f)
        {
            return MISS;
        }
        
        // exponential falloff per https://raytracing.github.io/books/RayTracingTheNextWeek.html#volumes/constantdensitymediums
        const auto random = glm::linearRand(0.f, 1.f);
        const auto travel = -(1.f / density) * glm::log(random);

        if (travel < scatter_distance)
        {
            // scatter randomly within the bounding media
            const auto position = entry + ray.direction * travel;
            const auto normal = glm::sphericalRand(1.f);

            const auto attenuation = glm::exp(-density * travel * material.albedo);
            material.albedo *= attenuation;

            return 
            {
                .position = position,
                .normal = normal,
                .material = material,
                .depth = glm::length(position - ray.origin),
                .hit = true,
                .object = this,
                .uv = { 0.f, 0.f },
            };
        }

        return MISS;
    }

    glm::vec3 Colloid::sample()
    {
        return container->sample();
    }

    glm::vec3 Colloid::normal_of(const glm::vec3& position)
    {
        return glm::sphericalRand(1.f);
    }

    // (c) Connor J. Link. Partial attribution (meaningful modifications performed herein) from personal work outside of ISU.
    // Utility function that does not meaningfully affect project functionality.
    MeshInstance load_obj(const std::string& filepath, const PBRMaterial& default_material)
    {
        Mesh objects{};

        std::ifstream file(filepath);
        if (!file.good())
        {
            return { glm::identity<glm::mat4>(), objects };
        }

        std::vector<glm::vec3> vertices{};

        std::string line;
        while (std::getline(file, line))
        {
            auto tokens = split(line, " ");
            if (tokens.empty()) 
            {
                continue;
            }

            // TODO: incorporate normals and normal smoothing as necessary
            // TODO: incorporate texture coordinates as necessary
            // TODO: incorporate material files as necessary

            if (tokens[0] == "v" && tokens.size() >= 4)
            {
                const auto x = std::stof(tokens[1]);
                const auto y = std::stof(tokens[2]);
                const auto z = std::stof(tokens[3]);

                vertices.emplace_back(x, y, z);
            }
            else if (tokens[0] == "f" && tokens.size() >= 4)
            {
                if (tokens.size() == 4)
                {
                    // triangle, 3 vertices to form the face
                    const auto face0 = std::stoi(tokens[1]);
                    const auto face1 = std::stoi(tokens[2]);
                    const auto face2 = std::stoi(tokens[3]);

                    objects.emplace_back(new Triangle
                    { 
                        vertices[face0 - 1], 
                        vertices[face1 - 1], 
                        vertices[face2 - 1], 
                        glm::vec2{ 0.f, 0.f }, 
                        glm::vec2{ 0.f, 1.f }, 
                        glm::vec2{ 1.f, 1.f }, 
                        default_material,
                    });
                }
                else if (tokens.size() == 5)
                {
                    // quadrilateral, 4 vertices to form the face
                    const auto face0 = std::stoi(tokens[1]);
                    const auto face1 = std::stoi(tokens[2]);
                    [[maybe_unused]] const auto face2 = std::stoi(tokens[3]);
                    const auto face3 = std::stoi(tokens[4]);

                    // OBJ format note: consecutive vertices connected in polygon specification per https://en.wikipedia.org/wiki/Wavefront_.obj_file
                    objects.emplace_back(new Quadrilateral
                    {
                        vertices[face0 - 1],
                        vertices[face1 - 1],
                        vertices[face3 - 1],
                        default_material,
                    });
                }
                
            }
        }

        return { transform, objects };
    }
}
