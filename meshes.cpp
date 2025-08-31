#include "meshes.h"

// meshes.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    MeshInstance icosphere(const PBRMaterial& material)
    {
        return load_obj("icosphere.obj", transform, material);
    }

    MeshInstance torus(const glm::mat4& transform, const PBRMaterial& material)
    {
        return load_obj("torus.obj", transform, material);
    }

    MeshInstance cube(const glm::mat4& transform, const PBRMaterial& material)
    {
        return load_obj("cube.obj", transform, material);
    }

    MeshInstance cylinder(const glm::mat4& transform, const PBRMaterial& material)
    {
        return load_obj("cylinder.obj", transform, material);
    }

    MeshInstance teapot(const glm::mat4& transform, const PBRMaterial& material)
    {
        return load_obj("teapot.obj", transform, material);
    }

    MeshInstance monkey(const glm::mat4& transform, const PBRMaterial& material)
    {
        return load_obj("monkey.obj", transform, material);
    }
}
