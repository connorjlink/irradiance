#ifndef IRRADIANCE_UTILITY_H
#define IRRADIANCE_UTILITY_H

#include <limits>
#include <ranges>
#include <string>
#include <vector>
#include <charconv>

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
        float timestamp = 0.f;
    };

    struct PBRMaterial
    {
        /* ONLY NEEDED FOR .OBJ FILE LOADING FOR MTL BACK-REFERENCE */
        std::string name;

        /* base color RGB */
        /* falloff-absorb toward color RGB in dielectrics */
        glm::vec3 albedo;

        /* light production RGB, 0 = absorber only */
        glm::vec3 emission;

        /* 0 = dielectric, 1 = metal */
        Real metallicity;

        /* varies dielectric intersection by Snell's law, default of 1.5 similar to real glass */
        Real refraction_index = 1.5f;

        /* 0 = isotropic roughness, 1 = anistropic roughness */
        Real anisotropy;

        /* 0 = smooth surface, 1 = fully rough surface */
        Real roughness;

        /* 0 = light cannot penetrate dielectric, 1 = dielectric does not impede light */
        Real transmission; 

        /* albedo sampled from this texture if specified */
        olc::Sprite* texture = nullptr;
    };

    struct Object;

    struct RayIntersection
    {
        glm::vec3 position;
        glm::vec3 normal;
        PBRMaterial material;
        Real depth = std::numeric_limits<float>::infinity();
        Real exit = 0.f;
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

    template<typename T, std::size_t N>
    class CircularBuffer
    {
    private:
        std::array<T, N> data;
        std::size_t index = 0;
        std::size_t count = 0;
    
    public:
        void push(const T& value)
        {
            data[index] = value;
            index = (index + 1) % N;
        
            if (count < N)
            {
                count++;
            }
        }
        
        void reset(std::size_t new_count)
        {
            index = 0;
            count = 0;
            for (auto& item : data)
            {
                item = {};
                item.resize(new_count);
            }
        }
    
        T& peek(size_t i = 0)
        {
            return data[(index + i) % N];
        }
    
        T& at(size_t i = 0)
        {
            return data[i];
        }
    
        std::size_t size() const
        {
            return count;
        }
    
    public:
        auto begin() { return data.begin(); }
        auto end() { return data.end(); }
    };

    // (c) Connor J. Link. Attribution from personal work outside of ISU.
    // Utility function that does not meaningfully affect project functionality.
    template<typename T>
    struct ParseResult
    {
    	bool success;
    	T result;
    };
    template<typename T>
    ParseResult<T> from_string(const std::string& input)
    {
    	T result{};
    	const char* begin = input.data();
    	const char* end = begin + input.size();
    	auto [ptr, ec] = std::from_chars(begin, end, result);
    	bool success = (ec == std::errc() && ptr == end);
    	return { success, result };
    }
}

#endif
