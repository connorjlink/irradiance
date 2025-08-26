#include <fstream>

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
        static constexpr auto MAX_DISTANCE = std::numeric_limits<Real>::infinity();

        auto distance = MAX_DISTANCE;

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
                };
            }
        }

        return MISS;
    }

    RayIntersection Triangle::intersect(const Ray& ray)
    {
        // Triangle intersection logic to be implemented
        return RayIntersection{};
    }

    RayIntersection Quadrilateral::intersect(const Ray& ray)
    {
        // Quadrilateral intersection logic to be implemented
        return RayIntersection{};
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

                triangles.emplace_back(new Triangle{ vertices[face0 - 1], vertices[face1 - 1], vertices[face2 - 1], material });
            }
        }

        return triangles;
    }
}
