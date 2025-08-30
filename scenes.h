#ifndef IRRADIANCE_SCENES_H
#define IRRADIANCE_SCENES_H

#include "renderer.h"
#include "textures.h"

// scenes.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    std::vector<Object*> test_spheres() 
    {
        return
        {
            new Sphere
            {
                glm::vec3{ 120.f, -120.f, 100.f },
                60.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 0.f, 0.f, 0.f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                    .texture = rock.get(),
                }
            },
            new Sphere
            {
                glm::vec3{ 120.f, -120.f, 0.f },
                50.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, .5f, .5f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 20.f, 20.f, 10.f },
                    .metallicity = 1.f,
                    .anisotropy = 0.f,
                    .roughness = 0.f,
                }
            },
            new Sphere
            {
                glm::vec3{ 0.f, -4.f, 5.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, .5f, .5f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 1.f,
                    .anisotropy = 0.f,
                    .roughness = 0.f,
                }
            },
            new Sphere
            {
                glm::vec3{ 6.f, -1.f, 5.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, .5f, .5f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 1.f,
                    .anisotropy = 0.f,
                    .roughness = .1f,
                }
            },
            new Sphere
            {
                glm::vec3{ 4.f, -1.f, 2.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 0.f, 0.f, 0.f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 1.f,
                    .anisotropy = 0.f,
                    .roughness = .8f,
                    .texture = gemstone.get(),
                }
            },
            new Sphere
            {
                glm::vec3{ 2.f, -1.f, 0.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 0.f, 0.f, 0.f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                    .texture = water.get(),
                }
            },
            new Sphere
            {
                glm::vec3{ -2.f, -1.f, -1.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 0.f, 0.f, 0.f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = .9f,
                    .anisotropy = 0.f,
                    .roughness = .9f,
                    .texture = wood.get(),
                }
            },
            new Sphere
            {
                glm::vec3{ -4.f, -1.f, 2.f },
                2.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ .56f, .518f, .835f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                    .texture = perlin_high.get(),
                }
            },
            new Sphere
            {
                glm::vec3{ -2.f, -.5f, 5.f },
                .5f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 0.f, 1.f, 0.f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 1.f,
                    .anisotropy = 0.f,
                    .roughness = 0.f,
                }
            },
            new Sphere
            {
                glm::vec3{ 0.f, -2.f, 5.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 1.f, 0.f, 0.f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = .25f,
                    .anisotropy = 0.f,
                    .roughness = .25f,
                }
            },
            new Sphere
            {
                glm::vec3{ 3.f, -1.5f, 5.f },
                1.5f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 0.f, 0.f, 1.f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = .8f,
                    .anisotropy = 0.f,
                    .roughness = .1f,
                }
            },
            new Sphere
            {
                glm::vec3{ 0.f, 1000.f, 5.f },
                1000.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ .25f, .5f, .75f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },
            new Triangle
            {
                glm::vec3{ -50.f, 2.f, 5.f },
                glm::vec3{ 50.f, -50.f, 50.f },
                glm::vec3{ 0.f, 2.f, 50.f },
                glm::vec2{ 0.f, 1.f },
                glm::vec2{ 1.f, 0.f },
                glm::vec2{ .5f, 1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, .5f, .5f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = .5f,
                    .anisotropy = 0.f,
                    .roughness = .5f,
                    .texture = gemstone.get(),
                }
            },
            new Quadrilateral
            {
                glm::vec3{ 50.f, 0.f, 0.f },
                glm::vec3{ 50.f, -50.f, 0.f },
                glm::vec3{ 50.f, 0.f, 50.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, .5f, .5f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                    .texture = wood.get(),
                }
            },
            new Quadrilateral
            {
                glm::vec3{ 50.f, 0.f, -50.f },
                glm::vec3{ 50.f, -50.f, -50.f },
                glm::vec3{ 50.f, 0.f, 0.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, .5f, .5f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                    .texture = perlin_low.get(),
                }
            },
            new Quadrilateral
            {
                glm::vec3{ 1.f, -10.f, -1.f },
                glm::vec3{ -1.f, -10.f, -1.f },
                glm::vec3{ 1.f, -10.f, 1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, .5f, .5f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                    .texture = water.get(),
                }
            }
        };
    };

    std::vector<Object*> cornell_box()
    {
        return
        {
            new Quadrilateral
            {
                glm::vec3{ 1.f, 1.f, 1.f },
                glm::vec3{ -1.f, 1.f, 1.f },
                glm::vec3{ 1.f, -1.f, 1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .75f, .25f, .25f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },
            new Quadrilateral
            {
                glm::vec3{ -1.f, 1.f, -1.f },
                glm::vec3{ 1.f, 1.f, -1.f },
                glm::vec3{ -1.f, -1.f, -1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .75f, .75f, .75f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },
            new Quadrilateral
            {
                glm::vec3{ -1.f, 1.f, 1.f },
                glm::vec3{ -1.f, 1.f, -1.f },
                glm::vec3{ -1.f, -1.f, 1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .25f, .75f, .25f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },
            new Quadrilateral
            {
                glm::vec3{ 1.f, 1.f, -1.f },
                glm::vec3{ 1.f, 1.f, 1.f },
                glm::vec3{ 1.f, -1.f, -1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .25f, .25f, .75f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },
            new Quadrilateral
            {
                glm::vec3{ -1.f, 1.f, -1.f },
                glm::vec3{ 1.f, 1.f, -1.f },
                glm::vec3{ -1.f, 1.f, 1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .75f, .75f, .75f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },
            new Quadrilateral
            {
                glm::vec3{ 1.f, -1.f, 1.f },
                glm::vec3{ -1.f, -1.f, 1.f },
                glm::vec3{ 1.f, -1.f, -1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .75f, .75f, .75f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },
            new Quadrilateral
            {
                glm::vec3{ .25f, -.99f, .25f },
                glm::vec3{ -.25f, -.99f, .25f },
                glm::vec3{ .25f, -.99f, -.25f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ 1.f, 1.f, 1.f },
                    .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                    .emission = glm::vec3{ 4e1f, 4e1f, 4e1f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            }
        };
    }
}

#endif
