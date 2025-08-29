#ifndef IRRADIANCE_RENDERER_H
#define IRRADIANCE_RENDERER_H

#include "utility.h"
#include "olcPixelGameEngine.h"

// renderer.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    struct Object
    {
    public:
        PBRMaterial material;

    public:
        Object(const PBRMaterial& material) 
            : material{ material }
        {
        }
        virtual ~Object() = default;

    public:
        virtual RayIntersection intersect(const Ray& ray) = 0;
    };

    struct Sphere : public Object
    {
    public:
        glm::vec3 center;
        Real radius;

    public:
        Sphere(glm::vec3 center, Real radius, const PBRMaterial& material)
            : center{ center }, radius{ radius }, Object{ material }
        {
        }

    public:
        RayIntersection intersect(const Ray& ray) override;
    };

    struct Triangle : public Object
    {
    public:   
        glm::vec3 v0, v1, v2;
        glm::vec2 uv0, uv1, uv2;
        glm::vec3 edge0, edge1;
        Real d00, d01, d11;
        Real denominator;
        glm::vec3 normal;

    public:
        Triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec2 uv0, glm::vec2 uv1, glm::vec2 uv2, const PBRMaterial& material)
            : v0{ v0 }, v1{ v1 }, v2{ v2 }, uv0{ uv0 }, uv1{ uv1 }, uv2{ uv2 }, Object{ material }
        {
            // cache to avoid recomputing per intersection
            edge0 = v1 - v0;
            edge1 = v2 - v0;
            d00 = glm::dot(edge0, edge0);
            d01 = glm::dot(edge0, edge1);
            d11 = glm::dot(edge1, edge1);
            denominator = (d00 * d11) - (d01 * d01);
            normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        }

    public:
        RayIntersection intersect(const Ray& ray) override;
    };

    struct Quadrilateral : public Object
    {
    public:
        // v0 is a shared origin like Q from Ray Tracing in One Weekend, v1 and v2 are edge vectors therefrom to form a parallelogram
        glm::vec3 v0, v1, v2;
        glm::vec3 normal;
        // Ax + By + Cz = D (constant)
        Real constant;
        glm::vec3 reciprocal;


    public:
        Quadrilateral(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, const PBRMaterial& material)
            : v0{ v0 }, Object{ material }
        {
            this->v1 = v0 - v1;
            this->v2 = v0 - v2;

            const auto orthogonal = glm::cross(this->v1, this->v2);
            normal = glm::normalize(orthogonal);
            constant = glm::dot(normal, v0);
            reciprocal = orthogonal / glm::dot(orthogonal, orthogonal);
        }
    
    public:
        RayIntersection intersect(const Ray& ray) override;
    }; 

    std::vector<Object*> load_obj(const std::string& filepath, const PBRMaterial& material);
}

#endif
