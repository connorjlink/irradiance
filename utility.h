#ifndef IRRADIANCE_UTILITY_H
#define IRRADIANCE_UTILITY_H

#include <glm/glm.hpp>

// utility.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    struct Ray
    {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    struct RayIntersection
    {

    };

    struct PBRMaterial
    {

    };

    struct Object
    {

    };

    struct Sphere : public Object
    {

    };

    struct Triangle : public Object
    {

    };

    struct Quadrilateral : public Object
    {

    };
}

#endif
