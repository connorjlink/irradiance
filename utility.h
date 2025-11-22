#ifndef IRRADIANCE_UTILITY_H
#define IRRADIANCE_UTILITY_H

#include <limits>
#include <ranges>
#include <string>
#include <vector>
#include <charconv>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/gtx/norm.hpp"

#include "olcPixelGameEngine.h"

// utility.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    // would double benefit accuracy meaningfully for the ray tracer?
    using Real = float;

    static constexpr Real EPSILON_F = .001f;
    static const glm::vec3 EPSILON = glm::vec3{ EPSILON_F, EPSILON_F, EPSILON_F };

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
        Real anisotropy = 0.f;

        /* direction of anisotropy bias in local space */
        /* { ±1, 0, 0 } = tangent, { 0, ±1, 0 } = bitangent */
        glm::vec2 grain;

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
        Ray incoming;
        Ray outgoing;
    };

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
    	const auto begin = input.data();
    	const auto end = begin + input.size();

    	auto [ptr, ec] = std::from_chars(begin, end, result);
    	auto success = (ec == std::errc() && ptr == end);

    	return { success, result };
    }

    inline Real compute_srgb_luminance(const glm::vec3& color)
    {
        // sRGB: https://ninedegreesbelow.com/photography/srgb-luminance.html
        return .2126f * color.r + .7152f * color.g + .0722f * color.b;
    }

    inline olc::Pixel color_to_pixel(const glm::vec3& color)
    {
        const auto incoming = glm::clamp(color, glm::vec3{ 0.f, 0.f, 0.f }, glm::vec3{ 1.f, 1.f, 1.f });
        const auto scaled = incoming * 255.f;

        const auto r = static_cast<std::uint8_t>(scaled.r);
        const auto g = static_cast<std::uint8_t>(scaled.g);
        const auto b = static_cast<std::uint8_t>(scaled.b);

        return olc::Pixel{ r, g, b, 255 };
    }

    inline glm::vec3 pixel_to_color(const olc::Pixel& pixel)
    {
        return glm::vec3
        {
            static_cast<float>(pixel.r) / 255.f,
            static_cast<float>(pixel.g) / 255.f,
            static_cast<float>(pixel.b) / 255.f
        };
    }

    inline glm::ivec2 vi2d_to_ivec2(const olc::vi2d& v)
    {
        return glm::ivec2{ v.x, v.y };
    }

    inline glm::vec2 compute_material_grain(const PBRMaterial& material)
    {
        if (material.grain != glm::vec2{ 0.f, 0.f })
        {
            // already defined in the material

            return glm::normalize(material.grain);
        }

        const auto texture = material.texture;
        if (!texture || texture->width <= 1 || texture->height <= 1)
        {
            // texture is too small to evaluate, fallback to tangent

            return glm::vec2{ 1.f, 0.f };
        }

        // compute average gradient using 2x2 tensors

        auto tensor = glm::mat2{ 0.f };
        auto samples = 0;

        const auto size = vi2d_to_ivec2(texture->Size());
        const auto stride = glm::max(glm::ivec2{ 1 }, size / 64);

        for (auto y = 1; y < texture->height - 1; y += stride.y)
        {
            for (auto x = 1; x < texture->width - 1; x += stride.x)
            {
                const auto Y00 = compute_srgb_luminance(pixel_to_color(texture->GetPixel(x - 1, y)));
                const auto Y10 = compute_srgb_luminance(pixel_to_color(texture->GetPixel(x + 1, y)));
                const auto Y01 = compute_srgb_luminance(pixel_to_color(texture->GetPixel(x, y - 1)));
                const auto Y11 = compute_srgb_luminance(pixel_to_color(texture->GetPixel(x, y + 1)));

                const auto mean_x = (Y10 - Y00) * .5f;
                const auto mean_y = (Y11 - Y01) * .5f;

                tensor[0][0] += mean_x * mean_x;
                tensor[0][1] += mean_x * mean_y;
                tensor[1][0] += mean_y * mean_x;
                tensor[1][1] += mean_y * mean_y;

                samples++;
            }
        }

        if (samples == 0)
        {
            return glm::vec2{ 1.f, 0.f };
        }

        tensor /= static_cast<float>(samples);

        const auto trace = tensor[0][0] + tensor[1][1];

        const auto determinant = glm::determinant(tensor);
        const auto discriminant = glm::max(trace * trace - .25f * determinant, 0.f);
        const auto root = glm::sqrt(discriminant);

        const auto lambda1 = (.5f * trace) + root;
        const auto lambda2 = (.5f * trace) - root;

        const auto a = tensor[0][0] - lambda2;
        const auto b = tensor[0][1];

        auto vector = glm::vec2{};
        if (glm::abs(a) + glm::abs(b) < EPSILON_F)
        {
            // degenerate since not linearly independent
            // lower energy axis is probably closest to isotropic

            vector = tensor[0][0] < tensor[1][1]
                ? glm::vec2{ 1.f, 0.f }
                : glm::vec2{ 0.f, 1.f };
        }
        else
        {
            vector = glm::vec2{ -b, a };
        }

        // re-normalize directions for positive tangent and bitangent bias

        if (vector.x < 0.f)
        {
            vector = -vector;
        }

        const auto length2 = glm::length2(vector);
        if (length2 < EPSILON_F)
        {
            return glm::vec2{ 1.f, 0.f };
        }

        return glm::normalize(vector);
    }
}

#endif
