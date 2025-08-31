#ifndef IRRADIANCE_UTILITY_H
#define IRRADIANCE_UTILITY_H

#include <limits>
#include <ranges>
#include <string>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"

#include "olcPixelGameEngine.h"

// utility.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    // would double benefit accuracy meaningfully for the ray tracer?
    using Real = float;

    struct Ray
    {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    struct PBRMaterial
    {
        /* base color RGB */
        glm::vec3 albedo;

        /* falloff-absorb toward color RGB in dielectrics */
        glm::vec3 absorption;

        /* light production RGB, 0 = absorber only */
        glm::vec3 emission;

        /* 0 = dielectric, 1 = metal */
        Real metallicity;

        /* varies dielectric intersection by Snell's law, default of 1/1.5 similar to real glass */
        Real refraction_index = .67f;

        /* 0 = isotropic roughness, 1 = anistropic roughness */
        Real anisotropy;

        /* 0 = smooth surface, 1 = fully rough surface */
        Real roughness;

        /* 0 = light cannot penetrate dielectric, 1 = dielectric does not impede light */
        Real transmission; 

        olc::Sprite* texture = nullptr;
    };

    struct Object;

    struct RayIntersection
    {
        glm::vec3 position;
        glm::vec3 normal;
        PBRMaterial material;
        Real depth = std::numeric_limits<float>::infinity();
        bool hit;
        Object* object = nullptr;
        glm::vec2 uv;
    };

    // (c) Connor J. Link. Attribution from personal work outside of ISU.
    // Utility function that does not meaningfully affect project.
    inline std::vector<std::string> split(std::string text, const std::string& delimiter)
	{
		return text
			| std::ranges::views::split(delimiter)
			| std::ranges::views::transform([](auto&& str) 
				{ return std::string_view(&*str.begin(), std::ranges::distance(str)); })
			| std::ranges::to<std::vector<std::string>>();
	}
}

#endif
