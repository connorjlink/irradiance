#ifndef IRRADIANCE_SCENES_H
#define IRRADIANCE_SCENES_H

#include "renderer.h"
#include "textures.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

// scenes.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    MeshInstance test_spheres() 
    {
        static const auto mesh = Mesh
        {
            new Sphere
            {
                glm::vec3{ 120.f, -120.f, 150.f },
                60.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 0.f, 0.f, 0.f },
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
                60.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ .1f, .1f, .8f },
                    .emission = glm::vec3{ 1e2f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 0.1f,
                }
            },
            new Sphere
            {
                glm::vec3{ 0.f, -6.f, 5.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, .5f, .5f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = .9f,
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
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = .2f,
                    .anisotropy = 0.f,
                    .roughness = .2f,
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
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                    .texture = perlin_high.get(),
                }
            },
            new Sphere
            {
                glm::vec3{ -8.f, -1.f, 4.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 1.f, 1.f, 1.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .refraction_index = -1.5f,
                    .anisotropy = 0.f,
                    .roughness = 0.f,
                    .transmission = 1.f,
                }
            },
            new Sphere
            {
                glm::vec3{ -8.f, -1.f, 6.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 1.f, 1.f, 1.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .refraction_index = 1.5f,
                    .anisotropy = 0.f,
                    .roughness = 0.f,
                    .transmission = 1.f,
                }
            },
            new Cuboid
            {
                glm::vec3{ -.5f, -4.5f, -.5f },
                glm::vec3{ .01f, 1.5f, 1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .7f, 1.f, .8f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .refraction_index = 1.85f,
                    .anisotropy = 0.f,
                    .roughness = .01f,
                    .transmission = .91f,
                }
            },
            new Cuboid
            {
                glm::vec3{ -.5f, -6.5f, -.5f },
                glm::vec3{ 1.f, 1.f, 1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, 1.f, .6f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .refraction_index = 1.01f,
                    .anisotropy = 0.f,
                    .roughness = .3f,
                    .transmission = 1.f,
                }
            },
            new Cuboid
            {
                glm::vec3{ -2.5f, -6.5f, -.5f },
                glm::vec3{ 1.f, 1.f, 1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, 1.f, .6f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .refraction_index = 1.76f,
                    .anisotropy = 0.f,
                    .roughness = 0.f,
                    .transmission = 1.f,
                }
            },
            new Colloid
            {
                .25f,
                new Cuboid
                {
                    glm::vec3{ 8.5f, -6.5f, -.5f },
                    glm::vec3{ 4.f, 4.f, 4.f },
                    PBRMaterial
                    {
                        .albedo = glm::vec3{ .5f, 1.f, .6f },
                        .emission = glm::vec3{ 0.f, 0.f, 0.f },
                        .metallicity = 0.f,
                        .anisotropy = 0.f,
                        .roughness = 0.f,
                    }
                },
            },
            new Sphere
            {
                glm::vec3{ -8.f, -4.f, 4.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ .56f, .518f, .835f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 1.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },
            new Quadric
            {
                // hyperbolic paraboloid https://en.wikipedia.org/wiki/Paraboloid
                1.f, -1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -1.f, 0.f,
                glm::vec3{ -20.f, -12.f, 0.f },
                glm::vec3{ 10.f, 10.f, 10.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .25f, .75, .4f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = .5f,
                    .refraction_index = 1.67f,
                    .anisotropy = 0.f,
                    .roughness = .1f,
                    .transmission = .1f,
                    .texture = perlin_low.get(),
                }
            },
            new Sphere
            {
                glm::vec3{ -8.f, -7.f, 4.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 1.f, .05f, .025f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .refraction_index = 1.1f,
                    .anisotropy = 0.f,
                    .roughness = .05f,
                    .transmission = .1f,
                }
            },
            new Sphere
            {
                glm::vec3{ -2.f, -.5f, 5.f },
                .5f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ .1f, 1.f, .1f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = .9f,
                    .anisotropy = 0.f,
                    .roughness = 0.f,
                }
            },
            new Colloid
            {   
                1.f,
                new Sphere
                {
                    glm::vec3{ -2.f, -3.5f, 5.f },
                    2.f,
                    PBRMaterial
                    {
                        .albedo = glm::vec3{ 1.f, 1.f, 1.f },
                        .emission = glm::vec3{ 0.f, 0.f, 0.f },
                        .metallicity = 0.f,
                        .anisotropy = 0.f,
                        .roughness = 0.f,
                    }
                }
            },
            new Sphere
            {
                glm::vec3{ 0.f, -2.f, 5.f },
                1.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ 1.f, .1f, .1f },
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
                    .albedo = glm::vec3{ .1f, .1f, 1.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = .8f,
                    .anisotropy = 0.f,
                    .roughness = .1f,
                }
            },
            new Sphere
            {
                glm::vec3{ 3.f, -4.5f, 5.f },
                1.5f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ .9f, .5f, .1f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = .9f,
                    .anisotropy = 0.f,
                    .roughness = .4f,
                }
            },
            new Sphere
            {
                glm::vec3{ 0.f, 1000.f, 5.f },
                1000.f,
                PBRMaterial
                {
                    .albedo = glm::vec3{ .25f, .5f, .75f },
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
                glm::vec3{ 50.f, -20.f, 0.f },
                glm::vec3{ 50.f, 0.f, 50.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, .5f, .5f },
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
                glm::vec3{ 50.f, -20.f, -50.f },
                glm::vec3{ 50.f, 0.f, 0.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .5f, .5f, .5f },
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
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 0.f,
                    .texture = water.get(),
                }
            }
        };

        return
        {
            glm::identity<glm::mat4>(),
            mesh,
        };
    };

    MeshInstance cornell_box()
    {
        static const auto mesh = Mesh
        {
            // left wall (red)
            new Quadrilateral
            {
                glm::vec3{ 1.f, 1.f, 1.f },
                glm::vec3{ 1.f, -1.f, 1.f },
                glm::vec3{ 1.f, 1.f, -1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ 1.f, .25f, .25f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },

            // right wall (green)
            new Quadrilateral
            {
                glm::vec3{ -1.f, 1.f, -1.f },
                glm::vec3{ -1.f, -1.f, -1.f },
                glm::vec3{ -1.f, 1.f, 1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .25f, 1.f, .25f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },

            // back wall (white)
            new Quadrilateral
            {
                glm::vec3{ -1.f, 1.f, -1.f },
                glm::vec3{ -1.f, -1.f, -.95f },
                glm::vec3{ 1.f, 1.f, -1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ 1.f, 1.f, 1.f },
                    .emission = glm::vec3{ 0.f, 0.f , 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },

            // floor (white)
            new Quadrilateral
            {
                glm::vec3{ -1.f, -1.f, -1.f },
                glm::vec3{ -1.f, -1.f, 1.f },
                glm::vec3{ 1.f, -1.f, -1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ 1.f, 1.f, 1.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },

            // ceiling (white)
            new Quadrilateral
            {
                glm::vec3{ -1.f, 1.f, -1.f },
                glm::vec3{ 1.f, 1.f, -1.f },
                glm::vec3{ -1.f, 1.f, 1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ 1.f, 1.f, 1.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },

            // front wall (blue)
            new Quadrilateral
            {
                glm::vec3{  2.f, 1.f, 1.f },
                glm::vec3{  2.f, -2.f, .95f },
                glm::vec3{ -1.f, 1.f, 1.f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .25f, .25f, 1.f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            },

            // light source (emissive white)
            new Quadrilateral
            {
                glm::vec3{ .25f, -.99f, .25f },
                glm::vec3{ -.25f, -.99f, .25f },
                glm::vec3{ .25f, -.99f, -.25f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ 1.f, 1.f, 1.f },
                    .emission = glm::vec3{ .2f },
                    .metallicity = 0.f,
                    .anisotropy = 0.f,
                    .roughness = 1.f,
                }
            }
        };

        return MeshInstance
        {
            glm::identity<glm::mat4>(),
            mesh,
        };
    }
}

#endif
