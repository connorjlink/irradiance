#include "meshes.h"

// meshes.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    std::vector<Object*> icosphere(const glm::mat4& transform)
    {
        return load_obj("icosphere.obj", transform, PBRMaterial
        {
            .albedo = glm::vec3{ 1.f, .6f, 0.f },
            .absorption = glm::vec3{ 0.f, 0.f, 0.f },
            .emission = glm::vec3{ 0.f, 0.f, 0.f },
            .metallicity = 0.f,
            .anisotropy = 0.f,
            .roughness = 1.f,
        });
    }

    std::vector<Object*> torus(const glm::mat4& transform)
    {
        return load_obj("torus.obj", transform, PBRMaterial
        {
            .albedo = glm::vec3{ 0.f, 1.f, 1.f },
            .absorption = glm::vec3{ 0.f, 0.f, 0.f },
            .emission = glm::vec3{ 0.f, 0.f, 0.f },
            .metallicity = 0.f,
            .anisotropy = 0.f,
            .roughness = 1.f,
        });
    }

    std::vector<Object*> cube(const glm::mat4& transform)
    {
        return load_obj("cube.obj", transform, PBRMaterial
        {
            .albedo = glm::vec3{ 1.f, 0.f, 1.f },
            .absorption = glm::vec3{ 0.f, 0.f, 0.f },
            .emission = glm::vec3{ 0.f, 0.f, 0.f },
            .metallicity = 0.f,
            .anisotropy = 0.f,
            .roughness = 0.f,
        });
    }

    std::vector<Object*> cylinder(const glm::mat4& transform)
    {
        return load_obj("cylinder.obj", transform, PBRMaterial
        {
            .albedo = glm::vec3{ 0.f, 1.f, 1.f },
            .absorption = glm::vec3{ 0.f, 0.f, 0.f },
            .emission = glm::vec3{ 0.f, 0.f, 0.f },
            .metallicity = 0.f,
            .anisotropy = 0.f,
            .roughness = 1.f,
            .texture = wood.get(),
        });
    }

    std::vector<Object*> teapot(const glm::mat4& transform)
    {
        return load_obj("teapot.obj", transform, PBRMaterial
        {
            .albedo = glm::vec3{ 1.f, 1.f, 1.f },
            .absorption = glm::vec3{ 0.f, 0.f, 0.f },
            .emission = glm::vec3{ 0.f, 0.f, 0.f },
            .metallicity = 1.f,
            .anisotropy = 0.f,
            .roughness = 0.f,
        });
    }

    std::vector<Object*> monkey(const glm::mat4& transform)
    {
        return load_obj("monkey.obj", transform, PBRMaterial
        {
            .albedo = glm::vec3{ 1.f, .5f, .5f },
            .absorption = glm::vec3{ 0.f, 0.f, 0.f },
            .emission = glm::vec3{ 0.f, 0.f, 0.f },
            .metallicity = 0.f,
            .anisotropy = 0.f,
            .roughness = 1.f,
        });
    }
}
