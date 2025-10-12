#ifndef IRRADIANCE_TRANSFORM_H
#define IRRADIANCE_TRANSFORM_H

#include "utility.h"

#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"

#include <vector>
#include <algorithm>
#include <numeric>

// transform.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    inline static constexpr Real LONG_WAVELENGTH   = 700.0f;
    inline static constexpr Real MEDIUM_WAVELENGTH = 550.0f;
    inline static constexpr Real SHORT_WAVELENGTH  = 450.0f;

    // will be useful for Gaussian spectral sensitivity functions
    inline static constexpr Real WAVELENGTH_DEVIATION = 40.0f;

    inline static const Real WAVELENGTHS[3] = { LONG_WAVELENGTH, MEDIUM_WAVELENGTH, SHORT_WAVELENGTH };

    inline static constexpr Real REFENCE_WAVELENGTH = SHORT_WAVELENGTH;
    inline static constexpr Real WAVELENGTH_RESOLUTION = 10.0f;
    inline static constexpr Real WAVELENGTH_DELTA = (LONG_WAVELENGTH - SHORT_WAVELENGTH) / WAVELENGTH_RESOLUTION;

    inline static constexpr int DIFFRACTION_RASTER_SIZE = 512;

    template<typename T>
    struct ImageBuffer
    {
    public:
        std::vector<T> data;
        std::uint32_t width{};
        std::uint32_t height{};

    public:
        ImageBuffer() = default;

        ImageBuffer(std::uint32_t width, std::uint32_t height)
            : data(width * height), width{ width }, height{ height }
        {
        }

        ImageBuffer(std::uint32_t width, std::uint32_t height, const T& initial)
            : data(width * height, initial), width{ width }, height{ height }
        {
        }

    public:
        constexpr T& operator[](std::size_t x, std::size_t y)
        {
            return data[y * width + x];
        }
    };


    // The following functions derive from the sRGB color space conversion formulae as described here
    // https://entropymine.com/imageworsener/srgbformula/
    inline float srgb_to_linear(Real encoded)
    {
        if (encoded <= .04045f) 
        {
            return encoded / 12.92f;
        }
        return std::pow((encoded + .055f) / 1.055f, 2.4f);
    }

    inline float linear_to_srgb(Real unencoded)
    {
        if (unencoded <= .0031308f) 
        {
            return unencoded * 12.92f;
        }
        return 1.055f * std::pow(unencoded, .0f / 2.4f) - .055f;
    }

    inline Real percentile(const std::vector<Real>& data, Real percent)
    {
        if (data.empty()) 
        {
            return 0.f;
        }

        percent = std::clamp(percent, 0.f, 100.f);
        const auto bucket = percent / 100.f;

        const auto index = static_cast<size_t>(std::floor(bucket * (data.size() - 1)));
        std::vector<Real> temporary = data;
        std::nth_element(temporary.begin(), temporary.begin() + index, temporary.end());
        return temporary[index];
    }

    inline void normalize_by_percentiles(std::vector<Real>& image, Real low_percentile, Real high_percentile)
    {
        const auto low = percentile(image, low_percentile);
        const auto high = percentile(image, high_percentile);

        const auto denom = (high > low) ? (high - low) : 1.f;
        
        for (auto& pixel : image)
        {
            const auto t = (pixel - low) / denom;
            pixel = std::clamp(t, 0.f, 1.f);
        }
    }

    inline ImageBuffer<Real> polygonal_aperture(std::uint32_t blades, Real rotation)
    {
        ImageBuffer<Real> result{ DIFFRACTION_RASTER_SIZE, DIFFRACTION_RASTER_SIZE, 1.0f };

        const auto rotation_radians = glm::radians(rotation);
        const auto maximum = glm::cos(glm::pi<Real>() / blades);
        
        for (auto y = 0; y < result.height; y++)
        {
            const auto yy = (y / (result.height - 1.f) * 2.f) - 1.f;

            for (auto x = 0; x < result.width; x++)
            {
                const auto xx = (x / (result.width - 1.f) * 2.f) - 1.f;

                glm::vec2 point(xx, yy);
                bool inside = true;
                for (auto i = 0; i < blades; i++)
                {
                    const auto theta = 2.f * glm::pi<float>() * i / static_cast<float>(blades) + rotation_radians;
                    glm::vec2 polar(std::cos(theta), std::sin(theta));

                    if (glm::dot(point, polar) > maximum) 
                    { 
                        inside = false; 
                        break;
                    }
                }

                result[x, y] = inside ? 1.f : .0f;
            }
        }

        return result;
    }

    inline void fftshift2D(std::vector<Real>& a, std::uint32_t height, std::uint32_t width)
    {
        // As described by https://docs.pytorch.org/docs/stable/generated/torch.fft.fftshift.html
        // centers the zero-frequency component in the resultant spectrum
        // out[x, y] = in[(x + w/2) % w, (y + h/2) % h]

        std::vector<Real> out(a.size());
        const auto half_y = height / 2, half_x = width / 2;

        for (auto y = 0; y < height; y++)
        {
            const auto yy = (y + half_y) % height;

            for (auto x = 0; x < width; x++)
            {
                const auto xx = (x + half_x) % width;
                out[y * width + x] = a[yy * width + xx];
            }
        }

        a.swap(out);
    }

    inline void ifftshift2D(std::vector<Real>& a, std::uint32_t height, std::uint32_t width)
    {
        // out[y,x] = in[(y + (h+1)/2) % h, (x + (w+1)/2) % w] (same as h/2 when even)

        std::vector<Real> out(a.size());
        const auto half_y = (height + 1) / 2, half_x = (width + 1) / 2;

        for (auto y = 0; y < height; y++)
        {
            const auto yy = (y + half_y) % height;

            for (auto x = 0; x < width; x++)
            {
                const auto xx = (x + half_x) % width;
                out[y * width + x] = a[yy * width + xx];
            }
        }

        a.swap(out);
    }

    inline std::vector<float> resize_bilinear(const std::vector<float>& source, std::uint32_t source_width, std::uint32_t source_height, std::uint32_t destination_width, std::uint32_t destination_height) 
    {
        // why doesn't this allow aggregate initialization???
        std::vector<Real> result(destination_width * destination_height, .0f);
        if (source_width == 0 || source_width == 0 || destination_width == 0 || destination_height == 0) 
        {
            return result;
        }

        const auto scale_x = source_width / static_cast<float>(destination_width);
        const auto scale_y = source_height / static_cast<float>(destination_height);

        for (auto y = 0; y < destination_height; y++)
        {
            const auto target_y = (y + .5f) * scale_y - .5f;

            auto y0 = static_cast<std::uint32_t>(std::floor(target_y));
            auto y1 = y0 + 1;

            const auto wy1 = target_y - y0;
            const auto wy0 = 1.f - wy1;

            y0 = std::clamp(y0, 0u, source_height - 1);
            y1 = std::clamp(y1, 0u, source_height - 1);

            for (auto x = 0; x < destination_width; x++)
            {
                const auto target_x = (x + 0.5f) * scale_x - 0.5f;

                auto x0 = static_cast<std::uint32_t>(std::floor(target_x));
                auto x1 = x0 + 1;

                const auto wx1 = target_x - x0;
                const auto wx0 = 1.f - wx1;
                
                x0 = std::clamp(x0, 0u, source_width - 1);
                x1 = std::clamp(x1, 0u, source_width - 1);

                const auto v00 = source[y0 * source_width + x0];
                const auto v01 = source[y0 * source_width + x1];
                const auto v10 = source[y1 * source_width + x0];
                const auto v11 = source[y1 * source_width + x1];

                const auto v0 = v00 * wx0 + v01 * wx1;
                const auto v1 = v10 * wx0 + v11 * wx1;
                result[y * destination_width + x] = v0 * wy0 + v1 * wy1;
            }
        }
        return result;
    }

    inline std::vector<Real> rescale_psf(const std::vector<Real>& psf, std::uint32_t psf_width, std::uint32_t psf_height, Real wavelength)
    {
        const auto scale = wavelength / SHORT_WAVELENGTH;

        const auto new_width = std::max(1, static_cast<int>(std::round(psf_width * scale)));
        const auto new_height = std::max(1, static_cast<int>(std::round(psf_height * scale)));
        
        std::vector<float> psf_scaled = resize_bilinear(psf, psf_height, psf_width, new_height, new_width);

        // normalize
        const auto sum = std::accumulate(psf_scaled.begin(), psf_scaled.end(), 0.0);
        if (sum > 0.f)
        {
            for (auto& pixel : psf_scaled) 
            {
                pixel /= sum;
            }
        }

        return psf_scaled;
    }
}

#endif 
