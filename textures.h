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
        using PerlinInterpolationArray = std::array<std::array<std::array<glm::vec3, INTERPOLATION_DELTA>, INTERPOLATION_DELTA>, INTERPOLATION_DELTA>;
        using TrilinearInterpolationArray = std::array<std::array<std::array<Real, INTERPOLATION_DELTA>, INTERPOLATION_DELTA>, INTERPOLATION_DELTA>;

    private:
        std::array<glm::vec<M, int>, N> permutation;
        std::array<glm::vec3, N> random;
        Real frequency = 1.f;
        Real amplitude = 1.f;

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

        template<std::size_t D>
        Real turbulence(const glm::vec<M, Real>& point) const
        {
            auto sum = 0.f;
            auto weight = 1.f;
            auto p = point;

            for (auto i = 0; i < D; i++)
            {
                sum += weight * noise(p);
                weight *= .5f;
                p *= 2.f;
            }

            return std::abs(sum);
        }

        Real perlinlerp(PerlinInterpolationArray& sample, const glm::vec<M, Real>& uvw, const glm::vec<M, Real>& uvw_smoothed) const
        {
            auto sum = 0.f;

            for (auto x = 0; x < INTERPOLATION_DELTA; x++)
            {
                for (int y = 0; y < INTERPOLATION_DELTA; y++)
                {
                    for (int z = 0; z < INTERPOLATION_DELTA; z++)
                    {
                        const auto weight = uvw - glm::vec<M, Real>{ x, y, z };
                        const auto dot = glm::dot(sample[x][y][z], weight);
                        
                        // Full attribution to Peter Shirley for Perlin interpolation formula!
                        // https://raytracing.github.io/books/RayTracingTheNextWeek.html#perlinnoise/usingrandomvectorsonthelatticepoints
                        sum += dot * 
                            (x * uvw_smoothed.x + (1.f - x) * (1.f - uvw_smoothed.x)) *
                            (y * uvw_smoothed.y + (1.f - y) * (1.f - uvw_smoothed.y)) * 
                            (z * uvw_smoothed.z + (1.f - z) * (1.f - uvw_smoothed.z)); 
                    }
                }
            }

            return sum;
        }

        // NOTE: old method, causes some clearly visible square artifacts
        Real trilerp(TrilinearInterpolationArray& sample, Real u, Real v, Real w) const
        {
            auto sum = 0.f;

            for (auto x = 0; x < INTERPOLATION_DELTA; x++)
            {
                for (int y = 0; y < INTERPOLATION_DELTA; y++)
                {
                    for (int z = 0; z < INTERPOLATION_DELTA; z++)
                    {
                        // Full attribution to Peter Shirley for trilinear interpolation formula!
                        // https://raytracing.github.io/books/RayTracingTheNextWeek.html#perlinnoise/smoothingouttheresult
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
            const auto ijk = glm::vec<M, int>{ glm::floor(point) };
        
            auto sample = PerlinInterpolationArray{};
            for (int x = 0; x < INTERPOLATION_DELTA; x++)
            {
                for (int y = 0; y < INTERPOLATION_DELTA; y++)
                {
                    for (int z = 0; z < INTERPOLATION_DELTA; z++)
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
            // Hermite cubic smoothing to reduce artifacting
            const auto uvw_smoothed = uvw * uvw * (3.f - 2.f * uvw);
            return perlinlerp(sample, uvw, uvw_smoothed);
        }

    public:
        PerlinNoise(Real frequency = 1.f, Real amplitude = 1.f)
            : frequency{ frequency }, amplitude{ amplitude }
        {
            // pre-populate a known sequence of random values
            std::transform(random.cbegin(), random.cend(), random.begin(), [&](auto)
            {
                return glm::sphericalRand(1.f);
            });

            generate();
        }

    public:
        olc::Pixel Sample(float u, float v, const glm::vec3& pos) const override
        {
            const auto value = 1.f + std::sin(frequency * amplitude * turbulence<5uz>(pos));
            const auto gray = static_cast<std::uint8_t>(value * 255.f);
            return olc::Pixel{ gray, gray, gray };
        }
    };

    inline std::unique_ptr<olc::Sprite> skybox;
    inline std::unique_ptr<olc::Sprite> water;
    inline std::unique_ptr<olc::Sprite> rock;
    inline std::unique_ptr<olc::Sprite> gemstone;
    inline std::unique_ptr<olc::Sprite> wood;
    inline std::unique_ptr<PerlinNoise<256uz, 3uz>> perlin_low;
    inline std::unique_ptr<PerlinNoise<256uz, 3uz>> perlin_high;

    inline void initialize_textures()
    {
        skybox = std::make_unique<olc::Sprite>("golden_gate_hills_4k.hdr");
        water = std::make_unique<olc::Sprite>("pexels-enginakyurt-1435752.jpg");
        rock = std::make_unique<olc::Sprite>("pexels-life-of-pix-8892.jpg");
        gemstone = std::make_unique<olc::Sprite>("pexels-jonnylew-1121123.jpg");
        wood = std::make_unique<olc::Sprite>("pexels-fwstudio-33348-129731.jpg");
        perlin_low = std::make_unique<PerlinNoise<256uz, 3uz>>(1.f, 10.f);
        perlin_high = std::make_unique<PerlinNoise<256uz, 3uz>>(10.f);
    }
}

#endif
