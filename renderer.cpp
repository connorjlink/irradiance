#include <fstream>
#include <cctype>
#include <filesystem>
#include <tuple>
#include <utility>
#include <map>
#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/compatibility.hpp"
#include "glm/gtc/random.hpp"

#include "olcPixelGameEngine.h"

#include "renderer.h"
#include "utility.h"

// renderer.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    const UniformSpherePDF Sphere::sphere_pdf = UniformSpherePDF{};

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

    glm::vec3 Sphere::normal_of(const glm::vec3& position)
    {
        return glm::normalize(position - center);
    }

    Real Sphere::evaluate(const RayIntersection&) const
    {
        return sphere_pdf.evaluate(glm::vec3{});
    }

    glm::vec3 Sphere::sample() const
    {
        return center + (radius * sphere_pdf.sample());
    }

    RayIntersection Triangle::intersect(const Ray& ray)
    {
        // Modified Möller-Trumbore from https://en.m.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm

        const auto test = glm::cross(ray.direction, edge1);

        // run Cramer's rule to intersect and get barycentric coordinates as UV
        const auto determinant = glm::dot(edge0, test);
        if (glm::abs(determinant) < EPSILON_F)
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
        if (t <= EPSILON_F)
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

        const auto uv = (w * uv0) + (u * uv1) + (v * uv2);

        return 
        {
            .position = intersection,
            .normal = smoothed_normal,
            .material = material,
            .depth = t,
            .exit = std::numeric_limits<Real>::infinity(),
            .hit = true,
            .object = this,
            .uv = uv,
        };
    }

    glm::vec3 Triangle::normal_of(const glm::vec3& position)
    {
        return normal;
    }

    Real Triangle::evaluate(const RayIntersection&) const
    {
        // uniform triangle PDF
        return 1.f / area;
    }

    glm::vec3 Triangle::sample() const
    {
        // compute as uniform barycentric coordinates, modified from 
        // https://stackoverflow.com/questions/4778147/sample-random-point-in-triangle
        const auto sqrt_r1 = glm::sqrt(glm::linearRand(0.f, 1.f));
        const auto r2 = glm::linearRand(0.f, 1.f);

        const auto u = 1.f - sqrt_r1;
        const auto v = r2 * sqrt_r1;

        return (1.f - u - v) * v0 + u * v1 + v * v2;
    }


    RayIntersection Quadrilateral::intersect(const Ray& ray)
    {
        // quad intersection from https://raytracing.github.io/books/RayTracingTheNextWeek.html
        // finds the plane containing the quad, then intersects the plane and verifies quad boundaries

        const auto denominator = glm::dot(normal, ray.direction);
        if (glm::abs(denominator) < EPSILON_F)
        {
            // parallel
            return MISS;
        }

        const auto t = (constant - glm::dot(normal, ray.origin)) / denominator;
        if (t <= EPSILON_F)
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

    glm::vec3 Quadrilateral::normal_of(const glm::vec3& position)
    {
        return normal;
    }

    Real Quadrilateral::evaluate(const RayIntersection& intersection) const
    {
        // https://raytracing.github.io/books/RayTracingTheRestOfYourLife.html#samplinglightsdirectly/gettingthepdfofalight

        const auto distance2 = intersection.depth * intersection.depth;
        const auto normal_angle = glm::abs(glm::dot(intersection.outgoing.direction, normal) / glm::length(intersection.outgoing.direction));

        return distance2 / (area * normal_angle);
    }

    glm::vec3 Quadrilateral::sample() const
    {
        // simple offsets into the parallelogram
        const auto u = glm::linearRand(0.f, 1.f);
        const auto v = glm::linearRand(0.f, 1.f);

        return v0 - u * v1 - v * v2;
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

            intersection += normal * EPSILON_F;

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

    glm::vec3 Cuboid::normal_of(const glm::vec3& position)
    {
        // compute unit-vector normals depending upon the face (since it is axis-aligned)

        if (glm::abs(position.x - origin.x) < EPSILON_F)
        {
            return glm::vec3{ -1.f, 0.f, 0.f };
        }
        else if (glm::abs(position.x - (origin.x + size.x)) < EPSILON_F)
        {
            return glm::vec3{ 1.f, 0.f, 0.f };
        }
        else if (glm::abs(position.y - origin.y) < EPSILON_F)
        {
            return glm::vec3{ 0.f, -1.f, 0.f };
        }
        else if (glm::abs(position.y - (origin.y + size.y)) < EPSILON_F)
        {
            return glm::vec3{ 0.f, 1.f, 0.f };
        }
        else if (glm::abs(position.z - origin.z) < EPSILON_F)
        {
            return glm::vec3{ 0.f, 0.f, -1.f };
        }
        else if (glm::abs(position.z - (origin.z + size.z)) < EPSILON_F)
        {
            return glm::vec3{ 0.f, 0.f, 1.f };
        }  

        return glm::vec3{ 0.f };
    }

    Real Cuboid::evaluate(const RayIntersection&) const
    {
        // uniform cuboid PDF
        return 1.f / area;
    }

    glm::vec3 Cuboid::sample() const
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

    Real Quadric::evaluate(const RayIntersection& direction) const
    {
        // uniform quadric PDF
        return 1.f / area;
    }

    glm::vec3 Quadric::sample() const
    {
        auto point = glm::vec3{};
        do
        {
            point = glm::linearRand(container->origin, container->origin + container->size);
        } 
        while (glm::abs(function(point)) > EPSILON_F);

        return point;
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

            const auto attenuation = glm::exp(-density * travel);
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

    glm::vec3 Colloid::normal_of(const glm::vec3& position)
    {
        return glm::sphericalRand(1.f);
    }

    Real Colloid::evaluate(const RayIntersection& direction) const
    {
        // uniform sub-object PDF
        return 1.f / container->area;
    }

    glm::vec3 Colloid::sample() const
    {
        return container->sample();
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
        auto local_space_exit = local_intersection.exit;

        if (local_space_exit == std::numeric_limits<float>::infinity())
        {
            const auto backface_ray = Ray
            {
                .origin = local_space_entry_position + local_space_ray.direction * EPSILON_F,
                .direction = local_space_ray.direction,
            };

            auto backcast = MISS;
            if (blas)
            {
                backcast = blas->intersect(backface_ray);
            }
            else
            {
                for (const auto& object : mesh)
                {
                    if (!object)
                    {
                        continue;
                    }

                    const auto intersection = object->intersect(backface_ray);
                    if (intersection.hit)
                    {
                        if (intersection.exit > backcast.exit)
                        {
                            backcast = intersection;
                        }
                    }
                }
            }

            if (backcast.hit)
            {
                local_space_exit = local_space_entry + EPSILON_F + backcast.exit;
            }
        }

        // transform relevant intersection space (object local coordinates) back into world space

        const auto normal_matrix = glm::transpose(glm::inverse(transform));

        const auto world_space_entry_position = glm::vec3{ transform * glm::vec4{ local_space_entry_position, 1.f } };
        local_intersection.position = world_space_entry_position;
        // IMPORTANT: DO NOT CHANGE W=0, OTHERWISE THE TRANSLATION GETS APPLIED AGAIN WITH BAD RESULTS!!!!
        local_intersection.normal = glm::normalize(glm::vec3{ normal_matrix * glm::vec4{ local_intersection.normal, 0.f } });

        local_intersection.depth = glm::length(world_space_entry_position - ray.origin);
        if (local_space_exit != std::numeric_limits<Real>::infinity())
        {
            const auto delta = local_space_exit - local_space_entry;
            const auto local_space_exit_position = local_space_entry_position + delta * local_space_ray.direction;
            const auto world_space_exit_position = glm::vec3{ transform * glm::vec4{ local_space_exit_position, 1.f } };
            local_intersection.exit = glm::length(world_space_exit_position - ray.origin);
        }

        return local_intersection;
    }

    // Full and complete credit to Nemo https://stackoverflow.com/questions/33511753/how-can-i-generate-a-tuple-of-n-type-ts
    template<typename T, unsigned N, typename... Ts>
    struct generate_tuple_type
    {
        typedef typename generate_tuple_type<T, N-1, T, Ts...>::type type;
    };

    template<typename T, typename... Ts>
    struct generate_tuple_type<T, 0, Ts...>
    {
        typedef std::tuple<Ts...> type;
    };


    template<typename U, typename... Ts>
    typename generate_tuple_type<U, sizeof...(Ts)>::type from_strings(Ts&&... ts)
    {
        auto arguments = std::forward_as_tuple(std::forward<Ts>(ts)...);

        return [&]<std::size_t... Is>(std::index_sequence<Is...>)
        {
            using tuple_t = typename generate_tuple_type<U, sizeof...(Ts)>::type;

            return tuple_t
            {
                ([&]() -> U
                {
                    const auto result = from_string<U>(std::get<Is>(arguments));
                    if (!result.success)
                    {
                        std::println("Warning: invalid 3-space co-ordinate `{}`", std::get<Is>(arguments));
                        return U{};
                    }

                    return result.result;
                })()...
            };
        }(std::make_index_sequence<sizeof...(Ts)>{});
    }

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
        std::vector<glm::vec2> texture_coordinates{};

        std::map<std::string, PBRMaterial> materials{};

        // NOTE: for simplicity, ignore groups and objects
        // NOTE: for simplicity, always assume "s 1" to avoid having to track per group or object

        std::map<std::string, olc::Sprite*> texture_cache{};

        auto load_texture = [&](const std::string& path)
        {
            if (!std::filesystem::exists(path))
            {
                std::println("Warning: non-existent texture file `{}`; falling back to default", path);
            }

            return new olc::Sprite{ path };
        };

        auto parse_and_emplace = [&]<typename T>(auto&&... arguments)
        {
            const auto result = from_strings<T>(std::forward<decltype(arguments)>(arguments)...);
            
            std::apply([&](auto&&... results)
            {
                vertices.emplace_back(std::forward<decltype(results)>(results)...);
            }, std::move(result));
        };

        // current material applied to each face; overwritten for each material file usage
        auto current_material = default_material;

        std::string line;
        while (std::getline(file, line))
        {
            auto tokens = split(line, " ");
            if (tokens.empty()) 
            {
                continue;
            }

            if (tokens[0] == "v" && tokens.size() >= 4)
            {
                // apple clang doesn't yet support C++17 std::from_chars<T> requires std::is_floating_point_v<T>(), so the following line does not work
                //parse_and_emplace.template operator()<Real>(tokens[1], tokens[2], tokens[3]);
                const auto x = std::stof(tokens[1]);
                const auto y = std::stof(tokens[2]);
                const auto z = std::stof(tokens[3]);
                vertices.emplace_back(x, y, z);
            }
            else if (tokens[0] == "vn" && tokens.size() >= 4)
            {
                // apple clang doesn't yet support C++17 std::from_chars<T> requires std::is_floating_point_v<T>(), so the following line does not work
                //const auto [x, y, z] = from_strings<Real>(tokens[1], tokens[2], tokens[3]);
                const auto x = std::stof(tokens[1]);
                const auto y = std::stof(tokens[2]);
                const auto z = std::stof(tokens[3]);
                normals.emplace_back(x, y, z);
            }
            else if (tokens[0] == "vt" && tokens.size() >= 3) 
            {
                // apple clang doesn't yet support C++17 std::from_chars<T> requires std::is_floating_point_v<T>(), so the following line does not work
                // const auto [u, v] = from_strings<Real>(tokens[1], tokens[2]);
                const auto u = stof(tokens[1]);
                const auto v = stof(tokens[2]);
                texture_coordinates.emplace_back(u, v);
            }
            else if (tokens[0] == "f" && tokens.size() >= 4)
            {
                if (tokens.size() == 4)
                {
                    // triangle, 3 vertices to form the face
                    // tokens could be v, v//n, v/t, or v/t/n

                    auto parse_vertex_index = [&](const std::string& s, int& vi, int& ni, int& ti)
                    {
                        vi = ni = ti = 0;

                        auto parts = split(s, "/");

                        if (parts.size() > 3)
                        {
                            std::println("Warning: invalid face vertex specification `{}`; skipping face", s);
                            vi = 0;
                            ni = 0;
                            return;
                        }

                        // handle vertex index
                        if (!parts.empty() && !parts[0].empty()) 
                        {
                            const auto result = from_string<int>(parts[0]);
                            if (!result.success)
                            {
                                std::println("Warning: invalid vertex index `{}` in face specification; skipping face", s);
                                vi = 0;
                                ni = 0;
                                return;
                            }

                            vi = result.result;
                        }

                        // handle texture coordinate if present
                        if ((parts.size() == 3 || parts.size() == 2) && !parts[1].empty())
                        {
                            const auto result = from_string<int>(parts[1]);
                            if (!result.success)
                            {
                                std::println("Warning: invalid texture coordinate index `{}` in face specification; skipping face", s);
                                vi = 0;
                                ni = 0;
                                return;
                            }

                            ti = result.result;
                        }

                        // handle vertex normal if present
                        if (parts.size() == 3 && !parts[2].empty()) 
                        {
                            const auto result = from_string<int>(parts[2]);
                            if (!result.success)
                            {
                                std::println("Warning: invalid normal index `{}` in face specification; skipping face", s);
                                vi = 0;
                                ni = 0;
                                return;
                            }

                            ni = result.result;
                        }
                        else if (parts.size() == 2 && !parts[1].empty() && ti == 0)
                        {
                            const auto result = from_string<int>(parts[1]);
                            if (!result.success)
                            {
                                std::println("Warning: invalid normal index `{}` in face specification; skipping face", s);
                                vi = 0;
                                ni = 0;
                                return;
                            }

                            ni = result.result;
                        }
                    };

                    auto v0 = 0, v1 = 0, v2 = 0;
                    auto n0 = 0, n1 = 0, n2 = 0;
                    auto t0 = 0, t1 = 0, t2 = 0;

                    parse_vertex_index(tokens[1], v0, n0, t0);
                    parse_vertex_index(tokens[2], v1, n1, t1);
                    parse_vertex_index(tokens[3], v2, n2, t2);

                    auto uv0 = glm::vec2{ 0.f, 0.f };
                    auto uv1 = glm::vec2{ 0.f, 1.f };
                    auto uv2 = glm::vec2{ 1.f, 1.f };

                    if (t0 > 0 && t0 <= static_cast<int>(texture_coordinates.size()) && 
                        t1 > 0 && t1 <= static_cast<int>(texture_coordinates.size()) && 
                        t2 > 0 && t2 <= static_cast<int>(texture_coordinates.size()))
                    {
                        uv0 = texture_coordinates[t0 - 1];
                        uv1 = texture_coordinates[t1 - 1];
                        uv2 = texture_coordinates[t2 - 1];
                    }

                    auto triangle = new Triangle
                    { 
                        vertices[v0 - 1], 
                        vertices[v1 - 1], 
                        vertices[v2 - 1], 
                        uv0,
                        uv1,
                        uv2,
                        current_material,
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
                    const auto face0 = from_string<int>(tokens[1]);
                    if (!face0.success || face0.result <= 0 || face0.result > static_cast<int>(vertices.size()))
                    {
                        std::println("Warning: invalid vertex index `{}` in face specification; skipping face", tokens[1]);
                        continue;
                    }

                    const auto face1 = from_string<int>(tokens[2]);
                    if (!face1.success || face1.result <= 0 || face1.result > static_cast<int>(vertices.size()))
                    {
                        std::println("Warning: invalid vertex index `{}` in face specification; skipping face", tokens[2]);
                        continue;
                    }

                    [[maybe_unused]] const auto face2 = from_string<int>(tokens[3]);
                    if (!face2.success || face2.result <= 0 || face2.result > static_cast<int>(vertices.size()))
                    {   
                        std::println("Warning: invalid vertex index `{}` in face specification; skipping face", tokens[3]);
                        continue;
                    }

                    const auto face3 = from_string<int>(tokens[4]);
                    if (!face3.success || face3.result <= 0 || face3.result > static_cast<int>(vertices.size()))
                    {
                        std::println("Warning: invalid vertex index `{}` in face specification; skipping face", tokens[4]);
                        continue;
                    }

                    // OBJ format note: consecutive vertices connected in polygon specification per https://en.wikipedia.org/wiki/Wavefront_.obj_file
                    objects.emplace_back(new Quadrilateral
                    {
                        vertices[face0.result - 1],
                        vertices[face1.result - 1],
                        vertices[face3.result - 1],
                        current_material,
                    });
                }
            }
            else if (tokens[0] == "usemtl")
            {
                const auto& material_name = tokens[1];

                if (materials.contains(material_name))
                {
                    const auto reference = materials.at(material_name);
                    current_material = reference;
                }
                else
                {
                    std::println("Warning: undefined material `{}`; falling back to default", material_name);
                    materials[material_name] = default_material;
                }
            }
            else if (tokens[0] == "mtllib")
            {
                const auto& material_library = tokens[1];

                std::ifstream material_file(material_library);
                if (!material_file.good())
                {
                    continue;
                }

                std::optional<PBRMaterial> material{};

                std::string material_line{};
                while (std::getline(material_file, material_line))
                {
                    if (material_line.starts_with("#") || material_line.empty())
                    {
                        continue;
                    }

                    const auto material_tokens = split(material_line, " ");
                    if (material_tokens.empty())
                    {
                        continue;
                    }

                    std::println("Material token[0]: {}", material_tokens[0]);
                    std::println("Material token size: {}", material_tokens.size());

                    // NOTE: for simplicity, ignoring Ka, Tf, 
                    // NOTE: for simplicity, ignoring illum, default to full PBR
                    // NOTE: for simplicity, ignoring map_Ka, map_Ks, map_Ns, map_d, map_bump, bump

                    if (material_tokens[0] == "newmtl")
                    {
                        if (material.has_value())
                        {
                            if (materials.contains(material->name))
                            {
                                std::println("Warning: duplicate material definition `{}`; overwriting previous definition", material->name);
                            }

                            materials[material->name] = *material;
                        }

                        if (material_tokens.size() >= 2)
                        {
                            material = PBRMaterial{};
                            material->name = material_tokens[1];
                        }
                        else
                        {
                            material = std::nullopt;
                        }
                    }
                    else if (material_tokens[0] == "Kd" || material_tokens[0] == "Ks") 
                    {
                        // albedo/diffuse colorization OR specular colorization

                        if (material_tokens.size() >= 4)
                        {
                            //const auto red = from_string<Real>(material_tokens[1]);
                            const auto red = ParseResult{ true, std::stof(material_tokens[1]) };
                            if (!red.success)
                            {
                                std::println("Warning: invalid red color `{}` in material `{}`", material_tokens[1], material.has_value() ? material->name : "<anonymous>");
                                continue;
                            }

                            //const auto green = from_string<Real>(material_tokens[2]);
                            const auto green = ParseResult{ true, std::stof(material_tokens[2]) };
                            if (!green.success)
                            {
                                std::println("Warning: invalid green color `{}` in material `{}`", material_tokens[2], material.has_value() ? material->name : "<anonymous>");
                                continue;
                            }

                            //const auto blue = from_string<Real>(material_tokens[3]);
                            const auto blue = ParseResult{ true, std::stof(material_tokens[3]) };
                            if (!blue.success)
                            {
                                std::println("Warning: invalid blue color `{}` in material `{}`", material_tokens[3], material.has_value() ? material->name : "<anonymous>");
                                continue;
                            }

                            if (material.has_value())
                            {
                                material->albedo = glm::clamp(glm::vec3{ red.result, green.result, blue.result }, glm::vec3{ 0.f }, glm::vec3{ 1.f });
                            }
                        }
                    }
                    else if (material_tokens[0] == "map_Kd")
                    {
                        // albedo/diffuse texture mapping

                        if (material_tokens.size() >= 2)
                        {
                            const auto texture_path = material_tokens[1];
                            if (material.has_value())
                            {
                                if (texture_cache.contains(texture_path))
                                {
                                    // intentionally leaking!
                                    material->texture = texture_cache.at(texture_path);
                                }
                                else
                                {
                                    texture_cache[texture_path] = load_texture(texture_path);
                                    material->texture = texture_cache.at(texture_path);
                                }

                                material->texture = load_texture(texture_path);
                            }
                        }
                    }
                    else if (material_tokens[0] == "d" || material_tokens[0] == "Tr")
                    {
                        // dissolve/transmission

                        if (material_tokens.size() >= 2)
                        {
                            auto transmission = 0.f;

                            if (material_tokens[0] == "d")
                            {
                                // dissolve
                                //const auto dissolve = from_string<Real>(material_tokens[1]);
                                const auto dissolve = ParseResult{ true, std::stof(material_tokens[1]) };
                                if (!dissolve.success)
                                {
                                    std::println("Warning: invalid dissolve `{}` in material `{}`", material_tokens[1], material.has_value() ? material->name : "<anonymous>");
                                    continue;
                                }

                                transmission = 1.f - dissolve.result;
                            }
                            else
                            {
                                // transparency = 1 - dissolve
                                //const auto dissolve = from_string<Real>(material_tokens[1]);
                                const auto dissolve = ParseResult{ true, std::stof(material_tokens[1]) };
                                if (!dissolve.success)
                                {
                                    std::println("Warning: invalid transparency `{}` in material `{}`", material_tokens[1], material.has_value() ? material->name : "<anonymous>");
                                    continue;
                                }

                                transmission = dissolve.result;
                            }

                            if (material.has_value())
                            {
                                material->transmission = glm::clamp(transmission, 0.f, 1.f);
                            }
                        }
                    }
                    else if (material_tokens[0] == "Ns")
                    {
                        // specular exponent, need to approximate map to roughness and metallic PBR parameters

                        if (material_tokens.size() >= 2)
                        {
                            //const auto specular_exponent = from_string<Real>(material_tokens[1]);
                            const auto specular_exponent = ParseResult{ true, std::stof(material_tokens[1]) };
                            if (!specular_exponent.success)
                            {
                                std::println("Warning: invalid specular exponent `{}` in material `{}`", material_tokens[1], material.has_value() ? material->name : "<anonymous>");
                                continue;
                            }

                            if (material.has_value())
                            {
                                const auto normalized = specular_exponent.result / 1000.f;
                                
                                material->metallicity = glm::clamp(normalized * normalized, 0.f, 1.f);
                                material->roughness = glm::clamp(1.f - glm::sqrt(normalized), 0.f, 1.f);
                            }
                        }
                    }
                    else if (material_tokens[0] == "Ni")
                    {
                        // index of refraction

                        if (material_tokens.size() >= 2)
                        {
                            //const auto refraction_index = from_string<Real>(material_tokens[1]);
                            const auto refraction_index = ParseResult{ true, std::stof(material_tokens[1]) };
                            if (!refraction_index.success)
                            {
                                std::println("Warning: invalid refraction index `{}` in material `{}`", material_tokens[1], material.has_value() ? material->name : "<anonymous>");
                                continue;
                            }

                            if (material.has_value())
                            {
                                material->refraction_index = refraction_index.result;
                            }
                        }
                    }
                    else if (material_tokens[0] == "Km")
                    {
                        // metallicity

                        if (material_tokens.size() >= 2)
                        {
                            //const auto metallicity = from_string<Real>(material_tokens[1]);
                            const auto metallicity = ParseResult{ true, std::stof(material_tokens[1]) };
                            if (!metallicity.success)
                            {
                                std::println("Warning: invalid metallicity `{}` in material `{}`", material_tokens[1], material.has_value() ? material->name : "<anonymous>");
                                continue;
                            }

                            if (material.has_value())
                            {
                                material->metallicity = glm::clamp(metallicity.result, 0.f, 1.f);
                            }
                        }
                    }
                }

                if (material.has_value())
                {
                    if (materials.contains(material->name))
                    {
                        std::println("Warning: duplicate material definition `{}`; overwriting previous definition", material->name);
                    }
                    materials[material->name] = *material;
                }
            }
        }

        return objects;
    }
}
