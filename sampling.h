#ifndef IRRADIANCE_SAMPLING_H
#define IRRADIANCE_SAMPLING_H

// sampling.h
// (c) 2025 Connor J. Link. All Rights Reserved.

#include "utility.h"

#include "glm/gtx/norm.hpp"

namespace ir
{
    class PDF
    {
    public:
        virtual Real evaluate(const glm::vec3& direction) const = 0;
        virtual glm::vec3 sample() const = 0;

    public:
        virtual ~PDF() = default;        
    };

    class CosinePDF : public PDF
    {
    private:
        glm::vec3 normal;
        glm::mat3 basis;

    public:
        CosinePDF(const glm::vec3& normal)
            : normal{ normal }
        {
            auto tangent = glm::normalize(glm::cross(normal, glm::vec3{ 0.f, 0.f, 1.f }));
            if (glm::length2(tangent) < EPSILON_F)
            {
                tangent = glm::normalize(glm::cross(normal, glm::vec3{ 0.f, 1.f, 0.f }));
            }

            const auto bitangent = glm::normalize(glm::cross(tangent, normal));
            basis = glm::mat3{ tangent, bitangent, normal };
        }

    public:
        Real evaluate(const glm::vec3& direction) const override
        {
            const auto cosine = glm::dot(glm::normalize(direction), glm::normalize(normal));
            return (cosine <= 0.f) ? 0.f : (cosine / glm::pi<Real>());
        }

        glm::vec3 sample() const override
        {
            const auto disk = glm::diskRand(1.f);
            const auto z = glm::sqrt(glm::clamp(1.f - disk.x * disk.x - disk.y * disk.y, 0.f, 1.f));
            const auto local_coordinates = glm::vec3{ disk.x, disk.y, z };

            const auto world_coordinates = glm::normalize(basis * local_coordinates);

            return world_coordinates;
        }
    };
}

#endif
