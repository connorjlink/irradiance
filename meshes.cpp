#include "meshes.h"

// meshes.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    std::vector<Object*> icosphere(const glm::mat4& transform, const PBRMaterial& material)
    {
        return load_obj("icosphere.obj", transform, material);
    }

    std::vector<Object*> torus(const glm::mat4& transform, const PBRMaterial& material)
    {
        return load_obj("torus.obj", transform, material);
    }

    std::vector<Object*> cube(const glm::mat4& transform, const PBRMaterial& material)
    {
        return load_obj("cube.obj", transform, material);
    }

    std::vector<Object*> cylinder(const glm::mat4& transform, const PBRMaterial& material)
    {
        return load_obj("cylinder.obj", transform, material);
    }

    std::vector<Object*> teapot(const glm::mat4& transform, const PBRMaterial& material)
    {
        return load_obj("teapot.obj", transform, material);
    }

    std::vector<Object*> monkey(const glm::mat4& transform, const PBRMaterial& material)
    {
        return load_obj("monkey.obj", transform, material);
    }
}
