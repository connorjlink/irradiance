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
        .exit = std::numeric_limits<ir::Real>::infinity(),
        .hit = false,
    };
}

namespace ir
{
    bool BoundingVolume::contains(const glm::vec3& point) const
    {
        return (point.x >= origin.x) && (point.x <= extent.x) &&
               (point.y >= origin.y) && (point.y <= extent.y) &&
               (point.z >= origin.z) && (point.z <= extent.z);
    }

    bool BoundingVolume::overlaps(const BoundingVolume& other) const
    {
        return (origin.x <= other.extent.x) && (extent.x >= other.origin.x) &&
               (origin.y <= other.extent.y) && (extent.y >= other.origin.y) &&
               (origin.z <= other.extent.z) && (extent.z >= other.origin.z);
    }

    bool BoundingVolume::intersects(const Ray& ray) const
    {
        // Modified slab method https://raytracing.github.io/books/RayTracingTheNextWeek.html#boundingvolumehierarchies/axis-alignedboundingboxes(aabbs)

        const auto t0 = (origin - ray.origin) / ray.direction;
        const auto t1 = (origin + size - ray.origin) / ray.direction;

        const auto entry = glm::min(t0, t1);
        const auto exit = glm::max(t0, t1);

        const auto tmin = glm::max(glm::max(entry.x, entry.y), glm::max(entry.z, 0.f));
        const auto tmax = glm::min(glm::min(exit.x, exit.y), exit.z);

        return tmax >= tmin;
    }

    BLAS::BLAS(const MeshInstance& meshes)
        : BLAS(meshes.mesh)
    {
    }

    BLAS::BLAS(const std::vector<Object*>& objects)
    {
        if (objects.empty())
        {
            return;
        }

        if (objects.size() == 1)
        {
            // leaf node
            const auto bounds = objects.front()->bounds();
            volume = new BoundingVolume{ bounds->origin, bounds->size, { objects.front() } };
            return;
        }

        // compute a bounding volume for all objects
        auto origin = glm::vec3{ std::numeric_limits<Real>::max() };
        auto extent = glm::vec3{ std::numeric_limits<Real>::min() };

        for (const auto& object : objects)
        {
            if (!object)
            {
                continue;
            }

            const auto bounds = object->bounds();
            origin = glm::min(origin, bounds->origin);
            extent = glm::max(extent, bounds->origin + bounds->size);
        }

        volume = new BoundingVolume{ origin, extent - origin };

        // split along the longest axis at the median
        const auto size = extent - origin;
        const auto axis = size.x > size.y ? (size.x > size.z ? 0 : 2) : (size.y > size.z ? 1 : 2);

        std::vector<Object*> left_objects;
        std::vector<Object*> right_objects;

        const auto midpoint = volume->centroid[axis];

        for (const auto& object : objects)
        {
            if (!object)
            {
                continue;
            }

            if (object->centroid[axis] < midpoint)
            {
                left_objects.push_back(object);
            }
            else
            {
                right_objects.push_back(object);
            }
        }

        // handle unbalanced splits by forcing a balanced split
        if (left_objects.empty() || right_objects.empty())
        {
            left_objects.clear();
            right_objects.clear();

            // IMPORTANT: HAVE TO SORT ALONG THE TARGET AXIS OTHERWISE THE SPLIT IS INVALID!! BAD ARTIFACTS!!!
            std::vector<Object*> sorted_objects = objects;
            std::sort(sorted_objects.begin(), sorted_objects.end(), [&](Object* left, Object* right)
            {
                return left->centroid[axis] < right->centroid[axis];
            });

            const auto half = objects.size() / 2;
            left_objects.insert(left_objects.end(), sorted_objects.begin(), sorted_objects.begin() + half);
            right_objects.insert(right_objects.end(), sorted_objects.begin() + half, sorted_objects.end());
        }

        left = new BLAS{ left_objects };
        right = new BLAS{ right_objects };
    }

    RayIntersection BLAS::intersect(const Ray& ray) const
    {
        if (!volume || !volume->intersects(ray))
        {
            return MISS;
        }

        if (!left && !right)
        {
            // leaf node, hit-test every object in this volume
            
            auto nearest_intersection = MISS;
            auto furthest_intersection = MISS;

            for (const auto& object : volume->contents)
            {
                if (!object)
                {
                    continue;
                }

                if (!object->bound->intersects(ray))
                {
                    continue;
                }

                const auto intersection = object->intersect(ray);

                if (intersection.hit && intersection.depth < nearest_intersection.depth)
                {
                    nearest_intersection = intersection;
                }
                if (intersection.hit && intersection.exit > furthest_intersection.exit)
                {
                    furthest_intersection = intersection;
                }
            }

            if (nearest_intersection.hit && furthest_intersection.hit && nearest_intersection.exit == std::numeric_limits<Real>::infinity())
            {
                nearest_intersection.exit = furthest_intersection.exit;
            }

            return nearest_intersection;
        }

        // branch node, traverse children (~log2 speedup)

        const auto left_intersection = left ? left->intersect(ray) : MISS;
        const auto right_intersection = right ? right->intersect(ray) : MISS;

        if (left_intersection.hit && right_intersection.hit)
        {
            return left_intersection.depth < right_intersection.depth ? left_intersection : right_intersection;
        }
        else if (left_intersection.hit)
        {
            return left_intersection;
        }
        else if (right_intersection.hit)
        {
            return right_intersection;
        }
        
        return MISS;
    }

    TLAS::TLAS(const std::vector<MeshInstance*>& meshes)
    {
        if (meshes.empty())
        {
            return;
        }

        if (meshes.size() == 1)
        {
            // leaf node
            volume = meshes.front()->volume;
            contents.push_back(meshes.front());
            return;
        }

        // compute a bounding volume for all mesh instances
        auto origin = glm::vec3{ std::numeric_limits<Real>::max() };
        auto extent = glm::vec3{ std::numeric_limits<Real>::min() };

        for (const auto& instance : meshes)
        {
            if (!instance || !instance->volume)
            {
                continue;
            }

            const auto bounds = instance->volume;
            origin = glm::min(origin, bounds->origin);
            extent = glm::max(extent, bounds->origin + bounds->size);
        }

        volume = new BoundingVolume{ origin, extent - origin };

        // split along the longest axis at the median
        const auto size = extent - origin;
        const auto axis = size.x > size.y ? (size.x > size.z ? 0 : 2) : (size.y > size.z ? 1 : 2);

        std::vector<MeshInstance*> left_instances;
        std::vector<MeshInstance*> right_instances;

        const auto midpoint = volume->centroid[axis];

        for (const auto& instance : meshes)
        {
            if (!instance || !instance->volume)
            {
                continue;
            }

            if (instance->volume->centroid[axis] < midpoint)
            {
                left_instances.push_back(instance);
            }
            else
            {
                right_instances.push_back(instance);
            }
        }

        // handle unbalanced splits by forcing a balanced split
        if (left_instances.empty() || right_instances.empty())
        {
            left_instances.clear();
            right_instances.clear();

            // IMPORTANT: HAVE TO SORT ALONG THE TARGET AXIS OTHERWISE THE SPLIT IS INVALID!! BAD ARTIFACTS!!!
            std::vector<MeshInstance*> sorted_instances = meshes;
            std::sort(sorted_instances.begin(), sorted_instances.end(), [&](MeshInstance* left, MeshInstance* right)
            {
                return left->volume->centroid[axis] < right->volume->centroid[axis];
            });

            const auto half = meshes.size() / 2;
            left_instances.insert(left_instances.end(), sorted_instances.begin(), sorted_instances.begin() + half);
            right_instances.insert(right_instances.end(), sorted_instances.begin() + half, sorted_instances.end());
        }

        left = new TLAS{ left_instances };
        right = new TLAS{ right_instances };
    }

    RayIntersection TLAS::intersect(const Ray& ray) const
    {
        if (!volume || !volume->intersects(ray))
        {
            return MISS;
        }

        if (!left && !right)
        {
            // leaf node, hit-test every mesh instance in this volume

            auto nearest_intersection = MISS;

            for (const auto& instance : contents)
            {
                if (!instance)
                {
                    continue;
                }

                if (!instance->volume->intersects(ray))
                {
                    continue;
                }

                const auto intersection = instance->intersect(ray);

                if (intersection.hit && intersection.depth < nearest_intersection.depth)
                {
                    nearest_intersection = intersection;
                }
            }

            return nearest_intersection;
        }

        // branch node, traverse children (~log2 speedup)

        const auto left_intersection = left ? left->intersect(ray) : MISS;
        const auto right_intersection = right ? right->intersect(ray) : MISS;

        if (left_intersection.hit && right_intersection.hit)
        {
            return left_intersection.depth < right_intersection.depth ? left_intersection : right_intersection;
        }
        else if (left_intersection.hit)
        {
            return left_intersection;
        }
        else if (right_intersection.hit)
        {
            return right_intersection;
        }

        return MISS;
    }

    BoundingVolume* Object::bounds()
    {
        return bound;
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

        const auto w = 1.f - u - v;

        auto smoothed_normal = normal;
        if (smoothed)
        {
            smoothed_normal = glm::normalize(w * n0 + u * n1 + v * n2);

            if (glm::dot(smoothed_normal, normal) < 0.f) 
            {
                smoothed_normal = -smoothed_normal;
            }
        }

        return 
        {
            .position = intersection,
            .normal = smoothed_normal,
            .material = material,
            .depth = t,
            .exit = std::numeric_limits<Real>::infinity(),
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
            .exit = std::numeric_limits<Real>::infinity(),
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
        const auto t2 = tmax;

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

            if (t1 > 0.f || t2 > 0.f)
            {
                auto intersection = ray.origin + ray.direction * t1;
                auto normal = normal_of(intersection);

                auto is_invalid = [&](const auto& intersection) 
                {
                    return glm::any(glm::lessThan(intersection, container->origin)) || 
                           glm::any(glm::greaterThan(intersection, container->origin + container->size));
                };


                // effectively clamp the quadric surface to the corresponding clip cube
                // NOTE: to fix back face collision, the intersection must be tested again against the back face collision (likely two hits per ray for generic quadrics)
                if (is_invalid(intersection))
                {
                    intersection = ray.origin + ray.direction * t2;

                    if (is_invalid(intersection))
                    {
                        return MISS;
                    }
                }

                if (glm::dot(normal, ray.direction) < 0.f)
                {
                    normal = -normal;
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

    RayIntersection MeshInstance::intersect(const Ray& ray) const
    {
        const auto local_space_ray = Ray
        {
            .origin = glm::vec3{ inverse * glm::vec4{ ray.origin, 1.f } },
            // IMPORTANT: DO NOT CHANGE W=0, OTHERWISE THE TRANSLATION GETS APPLIED AGAIN WITH BAD RESULTS!!!!
            .direction = glm::vec3{ inverse * glm::vec4{ ray.direction, 0.f } },
        };

        auto local_intersection = MISS;

        if (blas)
        {
            local_intersection = blas->intersect(local_space_ray);
        }
        else
        {
            auto nearest_intersection = RayIntersection{}, furthest_intersection = RayIntersection{};

            for (const auto& object : mesh)
            {
                if (!object)
                {
                    continue;
                }

                const auto intersection = object->intersect(local_space_ray);
                if (intersection.hit)
                {
                    // NOTE: not required to compute the world space position here because the BLAS intersection runs in this model space
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

            local_intersection = nearest_intersection;
        }

        if (!local_intersection.hit)
        {
            return MISS;
        }

        const auto local_space_entry_position = local_intersection.position;
        const auto local_space_entry = local_intersection.depth;
        const auto local_space_exit = local_intersection.exit;

        // transform relevant intersection space (object local coordinates) back into world space

        const auto world_space_entry_position = glm::vec3{ transform * glm::vec4{ local_space_entry_position, 1.f } };
        local_intersection.position = world_space_entry_position;
        // IMPORTANT: DO NOT CHANGE W=0, OTHERWISE THE TRANSLATION GETS APPLIED AGAIN WITH BAD RESULTS!!!!
        local_intersection.normal = glm::normalize(glm::vec3{ transform * glm::vec4{ local_intersection.normal, 0.f } });

        local_intersection.depth = glm::length(world_space_entry_position - ray.origin);
        if (local_space_exit != std::numeric_limits<float>::infinity())
        {
            const auto delta = local_space_exit - local_space_entry;
            const auto local_space_exit_position = local_space_entry_position + delta * local_space_ray.direction;
            const auto world_space_exit_position = glm::vec3{ transform * glm::vec4{ local_space_exit_position, 1.f } };
            local_intersection.exit = glm::length(world_space_exit_position - ray.origin);
        }

        return local_intersection;
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
        std::vector<glm::vec3> normals{};

        std::string line;
        while (std::getline(file, line))
        {
            auto tokens = split(line, " ");
            if (tokens.empty()) 
            {
                continue;
            }

            // TODO: incorporate texture coordinates as necessary
            // TODO: incorporate material files as necessary

            if (tokens[0] == "v" && tokens.size() >= 4)
            {
                const auto x = std::stof(tokens[1]);
                const auto y = std::stof(tokens[2]);
                const auto z = std::stof(tokens[3]);

                vertices.emplace_back(x, y, z);
            }
            else if (tokens[0] == "vn" && tokens.size() >= 4)
            {
                const auto x = std::stof(tokens[1]);
                const auto y = std::stof(tokens[2]);
                const auto z = std::stof(tokens[3]);

                normals.emplace_back(x, y, z);
            }
            else if (tokens[0] == "f" && tokens.size() >= 4)
            {
                if (tokens.size() == 4)
                {
                    // triangle, 3 vertices to form the face
                    // tokens could be v, v//n, v/t, or v/t/n

                    auto parse_vertex_index = [&](const std::string& s, int& vi, int& ni)
                    {
                        vi = ni = 0;

                        auto parts = split(s, "/");

                        if (!parts.empty() && !parts[0].empty()) 
                        {
                            vi = std::stoi(parts[0]);
                        }
                        if (parts.size() == 3 && !parts[2].empty()) 
                        {
                            ni = std::stoi(parts[2]);
                        }
                        else if (parts.size() == 2 && !parts[1].empty()) 
                        {
                            ni = std::stoi(parts[1]);
                        }
                    };

                    auto v0 = 0, v1 = 0, v2 = 0;
                    auto n0 = 0, n1 = 0, n2 = 0;

                    parse_vertex_index(tokens[1], v0, n0);
                    parse_vertex_index(tokens[2], v1, n1);
                    parse_vertex_index(tokens[3], v2, n2);

                    auto triangle = new Triangle
                    { 
                        vertices[v0 - 1], 
                        vertices[v1 - 1], 
                        vertices[v2 - 1], 
                        glm::vec2{ 0.f, 0.f }, 
                        glm::vec2{ 0.f, 1.f }, 
                        glm::vec2{ 1.f, 1.f }, 
                        default_material,
                    };

                    const auto have_all_normals = (n0 > 0 && n1 > 0 && n2 > 0 && 
                                                   n0 <= static_cast<int>(normals.size()) &&
                                                   n1 <= static_cast<int>(normals.size()) &&
                                                   n2 <= static_cast<int>(normals.size()));
                    if (have_all_normals)
                    {
                        triangle->n0 = normals[n0 - 1];
                        triangle->n1 = normals[n1 - 1];
                        triangle->n2 = normals[n2 - 1];
                        triangle->smoothed = true;
                    }

                    objects.emplace_back(triangle);
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
