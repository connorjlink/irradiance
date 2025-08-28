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
        virtual glm::vec2 compute_uv_coordinates(const glm::vec3& point) const = 0;
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
        glm::vec2 compute_uv_coordinates(const glm::vec3& point) const override;
    };

    struct Triangle : public Object
    {
    public:   
        glm::vec3 v0, v1, v2;

    public:
        Triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, const PBRMaterial& material)
            : v0{ v0 }, v1{ v1 }, v2{ v2 }, Object{ material }
        {
        }

    public:
        RayIntersection intersect(const Ray& ray) override;
        glm::vec2 compute_uv_coordinates(const glm::vec3& point) const override;
    };

    struct Quadrilateral : public Object
    {
    public:
        glm::vec3 v0, v1, v2, v3;

    public:
        Quadrilateral(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, const PBRMaterial& material)
            : v0{ v0 }, v1{ v1 }, v2{ v2 }, v3{ v3 }, Object{ material }
        {
        }
    
    public:
        RayIntersection intersect(const Ray& ray) override;
        glm::vec2 compute_uv_coordinates(const glm::vec3& point) const override;
    };

    std::vector<Triangle*> load_obj_as_triangles(const std::string& filepath, const PBRMaterial& material);
}

#endif
