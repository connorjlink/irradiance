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
        virtual RayIntersection intersect(const Ray& ray) = 0;
    };

    struct Sphere : public Object
    {
    public:
        glm::vec3 center;
        Real radius;

    public:
        RayIntersection intersect(const Ray& ray) override;
    };

    struct Triangle : public Object
    {
    public:   
        glm::vec3 v0, v1, v2;

    public:
        Triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, const PBRMaterial& material)
            : v0{ v0 }, v1{ v1 }, v2{ v2 }
        {
            this->material = material;
        }

    public:
        RayIntersection intersect(const Ray& ray) override;
    };

    struct Quadrilateral : public Object
    {
    public:
        glm::vec3 v0, v1, v2, v3;
    
    public:
        RayIntersection intersect(const Ray& ray) override;
    };

    std::vector<Triangle*> load_obj_as_triangles(const std::string& filepath, const PBRMaterial& material);
}

#endif
