#ifndef IRRADIANCE_TRANSFORM_H
#define IRRADIANCE_TRANSFORM_H

#include "utility.h"

#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"

#include <vector>
#include <algorithm>
#include <numeric>
#include <complex>
#include <print>

#include "/usr/local/include/fftw3.h"

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

    inline glm::vec3 srgb_to_linear(const glm::vec3& encoded)
    {
        return glm::vec3
        {
            srgb_to_linear(encoded.r),
            srgb_to_linear(encoded.g),
            srgb_to_linear(encoded.b)
        };
    }

    inline float linear_to_srgb(Real unencoded)
    {
        if (unencoded <= .0031308f) 
        {
            return unencoded * 12.92f;
        }
        return 1.055f * std::pow(unencoded, .0f / 2.4f) - .055f;
    }

    inline glm::vec3 linear_to_srgb(const glm::vec3& unencoded)
    {
        return glm::vec3
        {
            linear_to_srgb(unencoded.r),
            linear_to_srgb(unencoded.g),
            linear_to_srgb(unencoded.b)
        };
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

        const auto denominator = (high > low) ? (high - low) : 1.f;
        
        for (auto& pixel : image)
        {
            const auto t = (pixel - low) / denominator;
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

    inline void fftshift2D(std::vector<Real>& input, std::uint32_t height, std::uint32_t width)
    {
        // As described by https://docs.pytorch.org/docs/stable/generated/torch.fft.fftshift.html
        // centers the zero-frequency component in the resultant spectrum
        // out[x, y] = in[(x + w/2) % w, (y + h/2) % h]

        std::vector<Real> out(input.size());
        const auto half_y = height / 2, half_x = width / 2;

        for (auto y = 0; y < height; y++)
        {
            const auto yy = (y + half_y) % height;

            for (auto x = 0; x < width; x++)
            {
                const auto xx = (x + half_x) % width;
                out[y * width + x] = input[yy * width + xx];
            }
        }

        input.swap(out);
    }

    inline void ifftshift2D(std::vector<Real>& input, std::uint32_t height, std::uint32_t width)
    {
        // out[x, y] = in[(x + (w+1)/2) % w, (y + (h+1)/2) % h] (same as h/2 when even)

        std::vector<Real> out(input.size());
        const auto half_y = (height + 1) / 2, half_x = (width + 1) / 2;

        for (auto y = 0; y < height; y++)
        {
            const auto yy = (y + half_y) % height;

            for (auto x = 0; x < width; x++)
            {
                const auto xx = (x + half_x) % width;
                out[y * width + x] = input[yy * width + xx];
            }
        }

        input.swap(out);
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

    inline std::vector<std::complex<Real>> fft2_forward(const std::vector<Real>& input, std::uint32_t width, std::uint32_t height) 
    {
        std::vector<std::complex<Real>> result{ width * height , { 0.f, 0.f } };

        auto* in = (fftwf_complex*)fftw_malloc(sizeof(fftwf_complex) * width * height);
        auto* out = (fftwf_complex*)fftw_malloc(sizeof(fftwf_complex) * width * height);
        if (!in || !out) 
        {
            std::println("fftwf_malloc failed");
            return result;
        }

        for (auto i = 0; i < width * height; i++) 
        { 
            in[i][0] = input[i]; 
            in[i][1] = 0.f;
        }

        auto plan = fftwf_plan_dft_2d(width, height, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
        fftwf_execute(plan);

        for (auto i = 0; i < width * height; i++)
        {
            result[i] = { out[i][0], out[i][1] };
        }
        
        fftwf_destroy_plan(plan);
        fftwf_free(in);
        fftwf_free(out);

        return result;
    }

    inline std::vector<Real> ifft2_inverse_to_real(const std::vector<std::complex<Real>>& input, std::uint32_t width, std::uint32_t height)
    {
        std::vector<Real> result(width * height, 0.f);

        auto* in = (fftwf_complex*)fftw_malloc(sizeof(fftwf_complex) * width * height);
        auto* out = (fftwf_complex*)fftw_malloc(sizeof(fftwf_complex) * width * height);
        if (!in || !out) 
        {
            std::println("fftwf_malloc failed");
            return result;
        }

        for (int i = 0; i < width * height; i++) 
        { 
            in[i][0] = input[i].real(); 
            in[i][1] = input[i].imag(); 
        }

        auto plan = fftwf_plan_dft_2d(width, height, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
        fftwf_execute(plan);

        const auto scale = 1.f / (width * height);
        for (auto i = 0; i < width * height; i++) 
        {
            result[i] = out[i][0] * scale;
        }

        fftwf_destroy_plan(plan);
        fftwf_free(in);
        fftwf_free(out);

        return result;
    }
    
    // Great algorithm from https://stackoverflow.com/questions/364985/algorithm-for-finding-the-smallest-power-of-two-thats-greater-or-equal-to-a-giv
    // Full credit to Zacrath
    template<typename T> 
    T next_power2(T value)
    {
        return 1 << ((sizeof(T) * CHAR_BIT) - std::countl_zero(value - 1));
    }

    inline std::vector<Real> fft_convolve_same(ImageBuffer<Real>& image, ImageBuffer<Real>& kernel)
    {
        const auto Q = next_power2(image.width + kernel.width - 1);
        const auto P = next_power2(image.height + kernel.height - 1);

        std::vector<Real> buffer_a(P * Q, 0.f), buffer_b(P * Q, 0.f);
        for (auto y = 0; y < image.height; y++)
        {
            std::copy_n(&image.data[y * image.width], image.width, &buffer_a[y * Q]);
        }
        for (auto y = 0; y < kernel.height; y++)
        {
            std::copy_n(&kernel.data[y * kernel.width], kernel.width, &buffer_b[y * Q]);
        }

        // FFT
        // can save on zero-initialization since the fft will overwrite the full buffer
        std::vector<std::complex<Real>> fourier_a{ P * Q }, fourier_b{ P * Q };
        fourier_a = fft2_forward(buffer_a, (int)P, (int)Q);
        fourier_b = fft2_forward(buffer_b, (int)P, (int)Q);

        for (auto i = 0uz; i < fourier_a.size(); i++)
        {
            fourier_a[i] *= fourier_b[i];
        }

        // Inverse FFT to real (full conv result in top-left (H+KH-1)x(W+KW-1))
        std::vector<Real> convolution(P * Q, 0.f);
        convolution = ifft2_inverse_to_real(fourier_a, (int)P, (int)Q);

        // IMPORTANT: MUST CROP PER KERNEL CENTER ELSE THE RESULT IS MISALIGNED AND OUT-OF-BOUNDS
        const auto padding_x = (kernel.width - 1) / 2;
        const auto padding_y = (kernel.height - 1) / 2;

        std::vector<Real> out(image.width * image.height, 0.f);
        for (auto y = 0; y < image.height; ++y)
        {
            const auto shifted_y = y + padding_y;

            for (auto x = 0; x < image.width; ++x)
            {
                const auto shifted_x = x + padding_x;
                out[y * image.width + x] = convolution[shifted_y * Q + shifted_x];
            }
        }
        return out;
    }

    inline std::vector<Real> compute_psf(std::vector<Real>& aperture, std::uint32_t width, std::uint32_t height)
    {
        // IMPORTANT: ifftshift before FFT to avoid artifacts
        ifftshift2D(aperture, width, height);

        // FFT
        std::vector<std::complex<Real>> fourier(width * height);
        fourier = fft2_forward(aperture, width, height);

        // |FFT|^2
        std::vector<Real> fourier2(width * height, 0.f);
        for (auto i = 0; i < width * height; i++) 
        {
            fourier2[i] = std::norm(fourier[i]);
        }

        fftshift2D(fourier2, width, height);

        auto sum = std::accumulate(fourier2.begin(), fourier2.end(), 0.f);
        if (sum <= 0.f) 
        {
            std::println("Invalid PSF sum");
            return fourier2;
        }

        for (auto& pixel : fourier2) 
        {
            pixel /= sum;
        }

        return fourier2;
    }
}

#endif 
