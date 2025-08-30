#ifndef IRRADIANCE_TEXTURES_H
#define IRRADIANCE_TEXTURES_H

#include <memory>
#include <array>
#include <random>
#include <algorithm>
#include <cstdlib>

#include "utility.h"

#include "glm/glm.hpp"
#include "glm/gtc/random.hpp"
#include "olcPixelGameEngine.h"

// textures.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    template<std::size_t N, std::size_t M = 3>
        requires (std::popcount(N) == 1) // N must be a power of 2
    class PerlinNoise : public olc::Sprite
    {
    private:
        static constexpr std::size_t INTERPOLATION_DELTA = 2;
        using InterpolationArray = std::array<std::array<std::array<Real, INTERPOLATION_DELTA>, INTERPOLATION_DELTA>, INTERPOLATION_DELTA>;

    private:
        std::array<glm::vec<M, int>, N> permutation;
        std::array<Real, N> random;

    private:
        std::random_device device{};
        std::mt19937 generator{ device() };

    private:
        void generate()
        {
            std::iota(permutation.begin(), permutation.end(), glm::vec<3, int>{ 0, 0, 0 });
            permute();
        }

        void permute()
        {
            for (auto i = 0; i < N; i++)
            {
                for (auto j = 0; j < M; j++)
                {
                    std::uniform_int_distribution<> distribution(0, i);
                    const auto swapee = distribution(generator);
                    std::swap(permutation[i][j], permutation[swapee][j]);
                }
            }
        }

        Real trilerp(InterpolationArray& sample, Real u, Real v, Real w) const
        {
            auto sum = 0.f;

            for (auto x = 0; x < 2; x++)
            {
                for (int y = 0; y < 2; y++)
                {
                    for (int z = 0; z < 2; z++)
                    {
                        sum += sample[x][y][z] *
                            (x * u + (1.f - x) * (1.f - u)) *
                            (y * v + (1.f - y) * (1.f - v)) * 
                            (z * w + (1.f - z) * (1.f - w)); 
                    }
                }
            }

            return sum;
        }

    public:
        Real noise(const glm::vec<M, Real>& point) const
        {
            const auto ijk = glm::vec<3, int>{ glm::floor(point) };
        
            auto sample = InterpolationArray{};
            for (int x = 0; x < 2; x++)
            {
                for (int y = 0; y < 2; y++)
                {
                    for (int z = 0; z < 2; z++)
                    {
                        // "hashing" coordinates per RTTNW https://raytracing.github.io/books/RayTracingTheNextWeek.html#perlinnoise/usingblocksofrandomnumbers
                        const auto index = 
                            permutation[(ijk.x + x) & (N - 1)][0] ^
                            permutation[(ijk.y + y) & (N - 1)][1] ^
                            permutation[(ijk.z + z) & (N - 1)][2];
                        sample[x][y][z] = random[index];
                    }
                }
            }

            const auto uvw = point - glm::floor(point);
            return trilerp(sample, uvw.x, uvw.y, uvw.z);
        }

    public:
        PerlinNoise()
        {
            // pre-populate a known sequence of random values
            std::uniform_real_distribution<> distribution(0.f, 1.f);
            std::transform(random.cbegin(), random.cend(), random.begin(), [&](auto)
            {
                return distribution(generator);
            });

            generate();
        }

    public:
        olc::Pixel Sample(float u, float v, const glm::vec3& pos) const override
        {
            const auto value = noise(pos);
            const auto gray = static_cast<std::uint8_t>(value * 255.f);
            return olc::Pixel{ gray, gray, gray };
        }
    };

    inline std::unique_ptr<olc::Sprite> skybox;
    inline std::unique_ptr<olc::Sprite> water;
    inline std::unique_ptr<olc::Sprite> rock;
    inline std::unique_ptr<olc::Sprite> gemstone;
    inline std::unique_ptr<olc::Sprite> wood;
    inline std::unique_ptr<PerlinNoise<256uz, 3uz>> perlin;

    inline void initialize_textures()
    {
        skybox = std::make_unique<olc::Sprite>("golden_gate_hills_4k.hdr");
        water = std::make_unique<olc::Sprite>("pexels-enginakyurt-1435752.jpg");
        rock = std::make_unique<olc::Sprite>("pexels-life-of-pix-8892.jpg");
        gemstone = std::make_unique<olc::Sprite>("pexels-jonnylew-1121123.jpg");
        wood = std::make_unique<olc::Sprite>("pexels-fwstudio-33348-129731.jpg");
        perlin = std::make_unique<PerlinNoise<256uz, 3uz>>();
    }
}

#endif
