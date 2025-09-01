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
    bool BoundingVolume::contains(const glm::vec3& point) const
    {
        return (point.x >= origin.x) && (point.x <= origin.x + size.x) &&
               (point.y >= origin.y) && (point.y <= origin.y + size.y) &&
               (point.z >= origin.z) && (point.z <= origin.z + size.z);
    }

    bool BoundingVolume::intersects(const BoundingVolume& other) const
    {
        return (origin.x <= other.origin.x + other.size.x) && (origin.x + size.x >= other.origin.x) &&
               (origin.y <= other.origin.y + other.size.y) && (origin.y + size.y >= other.origin.y) &&
               (origin.z <= other.origin.z + other.size.z) && (origin.z + size.z >= other.origin.z);
    }

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
            const auto t2 = (-b + glm::sqrt(d)) / (2.f * a);

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

    BoundingVolume Sphere::bounds()
    {
        return BoundingVolume{ center - glm::vec3{ radius }, glm::vec3{ 2.f * radius } };
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

    BoundingVolume Triangle::bounds()
    {
        // check by complete extent
        const auto minimum = glm::min(v0, glm::min(v1, v2));
        const auto maximum = glm::max(v0, glm::max(v1, v2));

        return BoundingVolume{ minimum, maximum - minimum };
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

        return v0 - u * v1 - v * v2;
    }

    glm::vec3 Quadrilateral::normal_of(const glm::vec3& position)
    {
        return normal;
    }

    BoundingVolume Quadrilateral::bounds()
    {
        // check by complete extent
        const auto minimum = glm::min(v0, glm::min(glm::min(v0 + v1, v0 + v2), v0 + v1 + v2));
        const auto maximum = glm::max(v0, glm::max(glm::max(v0 + v1, v0 + v2), v0 + v1 + v2));

        return BoundingVolume{ minimum, maximum - minimum };
    }

    RayIntersection Cuboid::intersect(const Ray& ray)
    {
        // slab method https://en.wikipedia.org/wiki/Slab_method
        // modified into 3-D from https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection.html

        const auto minimum = origin;
        const auto maximum = origin + size;

        const auto reciprocal = 1.f / ray.direction;

        const auto f1 = (minimum.x - ray.origin.x) * reciprocal.x;
        const auto f2 = (maximum.x - ray.origin.x) * reciprocal.x;
        const auto f3 = (minimum.y - ray.origin.y) * reciprocal.y;
        const auto f4 = (maximum.y - ray.origin.y) * reciprocal.y;
        const auto f5 = (minimum.z - ray.origin.z) * reciprocal.z;
        const auto f6 = (maximum.z - ray.origin.z) * reciprocal.z;

        const auto tmin = glm::max(glm::max(glm::min(f1, f2), glm::min(f3, f4)), glm::min(f5, f6));
        const auto tmax = glm::min(glm::min(glm::max(f1, f2), glm::max(f3, f4)), glm::max(f5, f6));

        if (tmax < 0.f || tmin > tmax)
        {
            return MISS;
        }

        const auto t1 = tmin >= 0.f ? tmin : tmax;
        const auto t2 = tmin < 0.f ? tmax : tmin;

        if (t1 > 0.f)
        {
            auto intersection = ray.origin + ray.direction * t1;
            auto normal = normal_of(intersection);

            intersection += normal * .001f;

            const auto difference = intersection - centroid;
            // no idea if this is geometrically correct, but the same formula from the sphere seems to work okay :)
            const auto u = .5f + glm::atan2(difference.z, difference.x) / (2.f * glm::pi<Real>());
            const auto v = .5f + glm::asin(difference.y / glm::length(difference)) / glm::pi<Real>();

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

        return MISS;
    }

    glm::vec3 Cuboid::sample()
    {
        // choose a 2-D point from a random face
        const auto face = glm::linearRand(0, 6);
        const auto u = glm::linearRand(0.f, 1.f);
        const auto v = glm::linearRand(0.f, 1.f);

        switch (face)
        {
            case 0: return origin + glm::vec3{        0.f, u * size.y, v * size.z };
            case 1: return origin + glm::vec3{     size.x, u * size.y, v * size.z };
            case 2: return origin + glm::vec3{ u * size.x,        0.f, v * size.z };
            case 3: return origin + glm::vec3{ u * size.x,     size.y, v * size.z };
            case 4: return origin + glm::vec3{ u * size.x, v * size.y,        0.f };
            case 5: return origin + glm::vec3{ u * size.x, v * size.y,     size.z };
        }

        return origin;
    }

    glm::vec3 Cuboid::normal_of(const glm::vec3& position)
    {
        // compute unit-vector normals depending upon the face (since it is axis-aligned)

        if (glm::abs(position.x - origin.x) < .001f)
        {
            return glm::vec3{ -1.f, 0.f, 0.f };
        }
        else if (glm::abs(position.x - (origin.x + size.x)) < .001f)
        {
            return glm::vec3{ 1.f, 0.f, 0.f };
        }
        else if (glm::abs(position.y - origin.y) < .001f)
        {
            return glm::vec3{ 0.f, -1.f, 0.f };
        }
        else if (glm::abs(position.y - (origin.y + size.y)) < .001f)
        {
            return glm::vec3{ 0.f, 1.f, 0.f };
        }
        else if (glm::abs(position.z - origin.z) < .001f)
        {
            return glm::vec3{ 0.f, 0.f, -1.f };
        }
        else if (glm::abs(position.z - (origin.z + size.z)) < .001f)
        {
            return glm::vec3{ 0.f, 0.f, 1.f };
        }  

        return glm::vec3{ 0.f };
    }

    BoundingVolume Cuboid::bounds()
    {
        return BoundingVolume{ origin, size };
    }

    RayIntersection Quadric::intersect(const Ray& ray)
    {
        const auto& O = ray.origin;
        const auto& R = ray.direction;
        const auto& M = centroid;

        const auto a = (A * R.x * R.x) + (B * R.y * R.y) + (C * R.z * R.z) + 
                       (D * R.x * R.y) + (E * R.x * R.z) + (F * R.y * R.z);

        const auto b = (2.f * A * (O.x - M.x) * R.x) + (2.f * B * (O.y - M.y) * R.y) + (2.f * C * (O.z - M.z) * R.z) + 
                       (D * ((O.x - M.x) * R.y + (O.y - M.y) * R.x)) + 
                       (E * ((O.x - M.x) * R.z + (O.z - M.z) * R.x)) + 
                       (F * ((O.y - M.y) * R.z + (O.z - M.z) * R.y)) +
                       (G * R.x + H * R.y + I * R.z);

        const auto c = (A * (O.x - M.x) * (O.x - M.x)) + (B * (O.y - M.y) * (O.y - M.y)) + (C * (O.z - M.z) * (O.z - M.z)) + 
                       (D * (O.x - M.x) * (O.y - M.y)) +
                       (E * (O.x - M.x) * (O.z - M.z)) +
                       (F * (O.y - M.y) * (O.z - M.z)) +
                       (G * (O.x - M.x) + H * (O.y - M.y) + I * (O.z - M.z)) + J;

        const auto d = (b * b) - (4.f * a * c);

        if (d > 0.f)
        {
            const auto t1 = (-b - glm::sqrt(d)) / (2.f * a);
            const auto t2 = (-b + glm::sqrt(d)) / (2.f * a);

            if (t1 > 0.f)
            {
                const auto intersection = ray.origin + ray.direction * t1;
                auto normal = normal_of(intersection);

                // effectively clamp the quadric surface to the corresponding clip cube
                if (glm::any(glm::lessThan(intersection, container->origin)) || 
                    glm::any(glm::greaterThan(intersection, container->origin + container->size)))
                {
                    return MISS;
                }

                const auto difference = intersection - container->centroid;
                // no idea if this is geometrically correct, but the same formula from the sphere seems to work okay :)
                const auto u = .5f + glm::atan2(difference.z, difference.x) / (2.f * glm::pi<Real>());
                const auto v = .5f + glm::asin(difference.y / glm::length(difference)) / glm::pi<Real>();

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

    // TODO: proper sampling instead of random chance
    glm::vec3 Quadric::sample()
    {
        auto point = glm::vec3{};
        do
        {
            point = glm::linearRand(container->origin, container->origin + container->size);
        } 
        while (glm::abs(function(point)) > .001f);

        return point;
    }

    glm::vec3 Quadric::normal_of(const glm::vec3& position)
    {
        // derivatives of the quadric function
        // d/dx = 2Ax + Dy + Ez + G = 0
        // d/dy = 2By + Dx + Fz + H = 0
        // d/dz = 2Cz + Ex + Fy + I = 0

        return glm::normalize(glm::vec3
        {
            2.f * A * (position.x - centroid.x) + D * (position.y - centroid.y) + E * (position.z - centroid.z) + G,
            2.f * B * (position.y - centroid.y) + D * (position.x - centroid.x) + F * (position.z - centroid.z) + H,
            2.f * C * (position.z - centroid.z) + E * (position.x - centroid.x) + F * (position.y - centroid.y) + I,
        });
    }

    BoundingVolume Quadric::bounds()
    {
        return container->bounds();
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

    BoundingVolume Colloid::bounds()
    {
        return container->bounds();
    }

    RayIntersection MeshInstance::intersect(const Ray& ray) const
    {
        auto nearest_intersection = RayIntersection{}, furthest_intersection = RayIntersection{};

        auto is_composite_mesh = false;

        for (const auto& object : mesh)
        {
            if (!object)
            {
                continue;
            }

            // transform ray into object's local space
            const auto ray_transformed = Ray
            {
                .origin = glm::vec3{ inverse * glm::vec4{ ray.origin, 1.f } },
                // IMPORTANT: DO NOT CHANGE W=0, OTHERWISE THE TRANSLATION GETS APPLIED AGAIN WITH BAD RESULTS!!!!
                .direction = glm::vec3{ inverse * glm::vec4{ ray.direction, 0.f } },
            };

            auto intersection = object->intersect(ray_transformed);
            if (intersection.hit)
            {
                // transform relevant intersection space (object local coordinates) back into world space

                intersection.position = glm::vec3{ transform * glm::vec4{ intersection.position, 1.f } };
                // IMPORTANT: DO NOT CHANGE W=0, OTHERWISE THE TRANSLATION GETS APPLIED AGAIN WITH BAD RESULTS!!!!
                intersection.normal = glm::normalize(glm::vec3{ transform * glm::vec4{ intersection.normal, 0.f } });

                // TODO: ask Shaeffer why these are not required (produce shadow artifacts)
                // const auto entry_position = intersection.position;
                // const auto exit_position = intersection.position + ray_transformed.direction * (intersection.exit - intersection.depth);
                // intersection.depth = glm::length(glm::vec3{ instance.transform * glm::vec4{ entry_position, 1.f } } - ray.origin);
                // intersection.exit = glm::length(glm::vec3{ instance.transform * glm::vec4{ exit_position, 1.f } } - ray.origin);

                if (intersection.depth < nearest_intersection.depth)
                {
                    nearest_intersection = intersection;
                }
                else if (intersection.exit > furthest_intersection.exit)
                {
                    furthest_intersection = intersection;
                }
            }
        }

        if (nearest_intersection.hit && furthest_intersection.hit && nearest_intersection.exit == std::numeric_limits<float>::infinity())
        {
            nearest_intersection.exit = furthest_intersection.exit;
        }

        return nearest_intersection;
    }

    BoundingVolume MeshInstance::bounds() const
    {
        return volume;
    }

    // (c) Connor J. Link. Partial attribution (meaningful modifications performed herein) from personal work outside of ISU.
    // Utility function that does not meaningfully affect project functionality.
    Mesh load_obj(const std::string& filepath, const PBRMaterial& default_material)
    {
        Mesh objects{};

        std::ifstream file(filepath);
        if (!file.good())
        {
            return objects;
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

        return objects;
    }
}
