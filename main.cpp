// to silence intellisense errors
#define _LIBCPP_ENABLE_EXPERIMENTAL

#define OLC_PGE_APPLICATION
#define OLC_IMAGE_STB
#include "olcPixelGameEngine.h"

#include <charconv>
#include <string>
#include <algorithm>
#include <numeric>
#include <execution>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/random.hpp"

#include "utility.h"
#include "renderer.h"

// main.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

using namespace ir;

static constexpr Real MOUSE_SENSITIVITY = 5.f;
static constexpr Real MOVEMENT_SPEED = 5.f;
static constexpr glm::vec3 UP = glm::vec3{ 0.f, 1.f, 0.f };
static constexpr Real SAMPLE_JITTER = .001f;

static constexpr Real NONMETAL_REFLECTANCE = .04f;

static constexpr int FRAME_HISTORY = 5;

int _bounces = 2;
int _samples = 5;
int _captures = 1;

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
    
    void reset(std::size_t count)
    {
        index = 0;
        count = 0;
        for (auto& item : data)
        {
            item = {};
            item.resize(count);
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

class Irradiance : public olc::PixelGameEngine
{
public:
	Irradiance()
	{
		sAppName = "Irradiance";
	}

private:
    Sphere* sun = new Sphere
    {
        glm::vec3{ 100.f, -100.f, 0.f },
        10.f,
        PBRMaterial
        {
            .albedo = glm::vec3{ .8f, .8f, .8f },
            .absorption = glm::vec3{ 0.f, 0.f, 0.f },
            //.emission = glm::vec3{ .9f, .9f, .5f },
            .emission = glm::vec3{ 0.f, 0.f, 0.f },
            .metallicity = 0.f,
            .anisotropy = 0.f,
            .roughness = 0.f,
        }
    };

    std::vector<Object*> scene_objects = 
    {
        new Sphere
        {
            glm::vec3{ 120.f, -120.f, 0.f },
            100.f,
            PBRMaterial
            {
                .albedo = glm::vec3{ .5f, .5f, .5f },
                .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                .emission = glm::vec3{ 9.f, 9.f, 7.f },
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
            glm::vec3{ -2.f, 0.f, 5.f },
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
            glm::vec3{ 0.f, 0.f, 5.f },
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
            glm::vec3{ 3.f, 0.f, 5.f },
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
            glm::vec3{ 0.f, 1003.f, 5.f },
            1000.f,
            PBRMaterial
            {
                .albedo = glm::vec3{ .25f, .5f, .75f },
                .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                .emission = glm::vec3{ 0.f, 0.f, 0.f },
                .metallicity = 0.1f,
                .anisotropy = 0.f,
                .roughness = 1.f,
            }
        },
    };

private:
    olc::vi2d last_mouse_position = { 0, 0 };
    bool dirty = true;
    bool last_dirty = false;
    glm::vec3 position = { 0.f, 0.f, 0.f };
    Real fov_degrees = 90.f;
    Real yaw_degrees = 0.f;
    Real pitch_degrees = 0.f;
    int accumulated_frames = 1;
    Ray* rays = nullptr;
    bool highlight_dof = false;
    Real dof_distance = 0.f;

    glm::vec3* frame_buffer = nullptr;
    glm::vec3* staging_buffer = nullptr;

    CircularBuffer<std::vector<glm::vec3>, FRAME_HISTORY> frame_history;

    std::vector<int> index_buffer;

private:
    glm::vec3 compute_direction() const
    {
        const auto yaw_radians = glm::radians(yaw_degrees);
        const auto pitch_radians = glm::radians(pitch_degrees);

        const auto x = glm::cos(pitch_radians) * glm::sin(yaw_radians);
        const auto y = glm::sin(pitch_radians);
        const auto z = glm::cos(pitch_radians) * glm::cos(yaw_radians);

        return glm::normalize(glm::vec3{ x, y, z });
    }

    glm::vec3 compute_right() const
    {
        const auto direction = compute_direction();
        return glm::normalize(glm::cross(direction, UP));
    }

    glm::vec3 compute_average(std::size_t pixel)
    {
        auto result = glm::vec3{ 0.f };

        for (int i = 0; i < frame_history.size(); i++)
        {
            result += frame_history.at(i)[pixel];
        }
        result /= static_cast<Real>(FRAME_HISTORY);

        return result;
    }

    glm::vec3 trace(Ray& ray, int bounces)
    {
        if (bounces <= 0)
        {
            return glm::vec3{ 0.f };
        }

        glm::vec3 composite = glm::vec3{ 1.f };
        glm::vec3 radiance = glm::vec3{ 0.f };

        // TODO: bounding volume hierarchy acceleration structure

        // sort to find the nearest intersection for correct geometry rendering
        auto nearest_intersection = RayIntersection{};
        for (const auto& object : scene_objects)
        {
            const auto intersection = object->intersect(ray);

            if (intersection.hit && intersection.depth < nearest_intersection.depth)
            {
                nearest_intersection = intersection;
            }
        }

        if (nearest_intersection.hit)
        {
            //DrawRectDecal({ 1.f, 4.f }, { 2.f, 2.f }, olc::BLUE);

            // for testing only
            //std::cout << "Hit at depth: " << intersection.depth << "\n";

            // TODO: implement full PBR shading model

            if (nearest_intersection.material.emission != glm::vec3{ 0.f })
            {
                // emissive surfaces terminate bouncing
                return nearest_intersection.material.emission;
            }

        #if 0
            const auto difference = sun->center - nearest_intersection.position;
            auto in_shadow = false;
            for (const auto& shadow_candidate : scene_objects)
            {
                const auto shadow_ray = Ray{ nearest_intersection.position + nearest_intersection.normal * .001f, glm::normalize(difference) };
                const auto shadow_intersection = shadow_candidate->intersect(shadow_ray);

                if (shadow_intersection.hit)
                {
                    in_shadow = true;
                    break;
                }
            }
            if (in_shadow)
            {
                const auto direction = glm::normalize(difference);
                const auto angle = glm::max(glm::dot(direction, nearest_intersection.normal), 0.f);
                const auto light_contribution = sun->material.emission * (1.f - angle);

                composite *= light_contribution;
            }
        #endif
                
            const auto reflection = glm::reflect(ray.direction, nearest_intersection.normal);

            auto random_in_unit_sphere = glm::sphericalRand(1.f);
            if (glm::dot(random_in_unit_sphere, nearest_intersection.normal) < 0.f)
            {
                // needs to always face outward relative to the surface normal
                random_in_unit_sphere = -random_in_unit_sphere;
            }

            // move to next bounce
            ray.origin = nearest_intersection.position + nearest_intersection.normal * .001f; // offset to prevent self-intersection

            // TODO: incorporate importance sampling / probabilistic reflection

            // Fresnel term with Schlick's approximation
            const auto F0 = glm::mix(glm::vec3{ NONMETAL_REFLECTANCE }, nearest_intersection.material.albedo, nearest_intersection.material.metallicity);
            // negative direction for non-incident ray
            const auto angle = glm::clamp(glm::dot(-ray.direction, nearest_intersection.normal), 0.f, 1.f);
            const auto F = F0 + (glm::vec3{ 1.f } - F0) * glm::pow(1.f - angle, 5.f); 

            ray.direction =  glm::normalize(reflection + random_in_unit_sphere * nearest_intersection.material.roughness);
            const auto specular = F * trace(ray, bounces - 1);

            // Lambertian diffuse https://en.wikipedia.org/wiki/Lambertian_reflectance
            const auto brdf = (1.f - nearest_intersection.material.metallicity) * nearest_intersection.material.albedo / glm::pi<Real>();
            // TODO: replace with a proper BRDF
            const auto diffuse = brdf * trace(ray, bounces - 1);

            composite *= diffuse + specular;

            radiance += composite;

            // TODO: cast rays toward each surface (and emitter) for global illumination
        }
        else
        {
            // TODO: environment cube map sampling
            // accent color for sky
            //radiance += composite * glm::vec3{ 143.f / 255.f, 132.f / 255.f, 213.f / 255.f };
            radiance += composite * glm::vec3{ 0.f, 0.f, 0.f };
        }

        return radiance;
    };

public:
	bool OnUserCreate() override
	{
        const auto number = ScreenWidth() * ScreenHeight();
        
        rays = new Ray[number];
        frame_buffer = new glm::vec3[number];
        staging_buffer = new glm::vec3[number];

        for (auto& frame : frame_history)
        {
            frame.resize(number, glm::vec3{ 0.f });
        }

        index_buffer.resize(number, 0);
        std::iota(index_buffer.begin(), index_buffer.end(), 0);

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
        DrawStringPropDecal({ 5.f, 5.f }, std::format("Frames: {}, ms/frame: {:.2f}", accumulated_frames, fElapsedTime * 1000.f), olc::YELLOW);
        DrawStringPropDecal({ 5.f, 15.f }, std::format("Position: ({:.2f}, {:.2f}, {:.2f})", position.x, position.y, position.z), olc::YELLOW);
        DrawStringPropDecal({ 5.f, 25.f }, std::format("Yaw: {:.2f} Pitch: {:.2f}", yaw_degrees, pitch_degrees), olc::YELLOW);
        DrawStringPropDecal({ 5.f, 35.f }, std::format("DOF: {} @ {:.2f}", highlight_dof ? "ON" : "OFF", dof_distance), olc::YELLOW);

        const auto& ray = rays[GetMouseX() + GetMouseY() * ScreenWidth()].direction;
        DrawStringPropDecal({ 5.f, 45.f }, std::format("Ray: ({:2f}, {:2f}, {:2f})", ray.x, ray.y, ray.z), olc::YELLOW);

        if (GetMouse(olc::Mouse::RIGHT).bHeld)
        {
            const auto delta = GetMousePos() - last_mouse_position;

            yaw_degrees -= static_cast<Real>(delta.x) * fElapsedTime * MOUSE_SENSITIVITY;

            pitch_degrees += static_cast<Real>(delta.y) * fElapsedTime * MOUSE_SENSITIVITY;
            // prevents gimbal lock because the direction uses Euler angles
            pitch_degrees = glm::clamp(pitch_degrees, -80.f, 80.f);

            last_mouse_position = GetMousePos();

            dirty = true;
        }

        auto movement_speed = MOVEMENT_SPEED * (GetKey(olc::Key::CTRL).bHeld ? 5.f : 1.f);

        if (GetKey(olc::Key::W).bHeld)
        {
            position += compute_direction() * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::S).bHeld)
        {
            position -= compute_direction() * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::A).bHeld)
        {
            const auto right = compute_right();
            position -= right * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::D).bHeld)
        {
            const auto right = compute_right();
            position += right * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::SPACE).bHeld)
        {
            position -= UP * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::C).bHeld)
        {
            position += UP * fElapsedTime * movement_speed;
            dirty = true;
        }

        if (GetKey(olc::Key::TAB).bPressed)
        {
            highlight_dof = !highlight_dof;
        }
        if (GetKey(olc::Key::UP).bHeld)
        {
            dof_distance += fElapsedTime * movement_speed * 2.f;
            dirty = true;
        }
        if (GetKey(olc::Key::DOWN).bHeld)
        {
            dof_distance -= fElapsedTime * movement_speed * 2.f;
            dof_distance = glm::max(dof_distance, 0.f);
            dirty = true;
        }

        // PARALLELIZE

        std::for_each(std::execution::par, index_buffer.begin(), index_buffer.end(), [&](int i)
        {
            const auto x = i % ScreenWidth();
            const auto y = i / ScreenWidth();

            const auto& ray = rays[i];

            glm::vec3 total_color = glm::vec3{ 0.f, 0.f, 0.f };

            for (int s = 0; s < _samples; s++)
            {
                // using linear to avoid biasing sampling toward the center of each pixel
                auto ray_jittered = ray;
                ray_jittered.direction += glm::linearRand(glm::vec3{ -SAMPLE_JITTER, -SAMPLE_JITTER, -SAMPLE_JITTER }, glm::vec3{ SAMPLE_JITTER, SAMPLE_JITTER, SAMPLE_JITTER });

                total_color += trace(ray_jittered, _bounces);
            }

            total_color /= static_cast<Real>(_samples);
            staging_buffer[i] = total_color;
            frame_buffer[i] += total_color;
        });

        if (!dirty && last_dirty)
        {
            const auto count = ScreenWidth() * ScreenHeight();

            memcpy(frame_buffer, staging_buffer, sizeof(glm::vec3) * count);
            memset(staging_buffer, 0, sizeof(glm::vec3) * count);

            accumulated_frames = 1;
            frame_history.reset(count);
        }

        if (dirty)
        {
            DrawRectDecal({ 1.f, 1.f }, { 2.f, 2.f }, olc::GREEN);

            // recalculate the camera rays

            const auto aspect_ratio = static_cast<Real>(ScreenWidth()) / static_cast<Real>(ScreenHeight());
            const auto fov_radians = glm::radians(fov_degrees);

            const auto right = compute_right();

            const auto projection = glm::perspective(fov_radians, aspect_ratio, .1f, 1000.f);
            const auto inverse_projection = glm::inverse(projection);
            const auto view = glm::lookAt(position, position + compute_direction(), UP);
            const auto inverse_view = glm::inverse(view);

            for (int x = 0; x < ScreenWidth(); x++)
            {
                for (int y = 0; y < ScreenHeight(); y++)
                {
                    const auto u = static_cast<Real>(x) / static_cast<Real>(ScreenWidth());
                    const auto v = static_cast<Real>(y) / static_cast<Real>(ScreenHeight());

                    // normalized device coordinates
                    const auto ndc_x = 2.f * u - 1.f;
                    const auto ndc_y = 2.f * v - 1.f;

                    const auto clip_space_pos = glm::vec4{ ndc_x, ndc_y, 1.f, 1.f };
                    const auto world_space_pos_homogeneous = inverse_projection * clip_space_pos;
                    const auto world_space_pos = world_space_pos_homogeneous / world_space_pos_homogeneous.w;
                    const auto world_space_pos_normalized = inverse_view * glm::normalize(glm::vec4{ glm::vec3{ world_space_pos }, 0.f });

                    rays[y * ScreenWidth() + x] = Ray{ position, world_space_pos_normalized };
                }
            }

            for (int x = 0; x < ScreenWidth(); x++)
            {
                for (int y = 0; y < ScreenHeight(); y++)
                {
                    const auto color = compute_average(x + y * ScreenWidth());

                    // Reinhard filter https://en.wikipedia.org/wiki/Tone_mapping
                    const auto tone_mapped = color / (color + glm::vec3{ 1.f });
                    // Gamma correction, 2.2 common for sRGB https://en.wikipedia.org/wiki/Gamma_correction
                    const auto gamma_corrected = glm::pow(tone_mapped, glm::vec3{ 1.f / 2.2f });

                    Draw(x, y, olc::Pixel(gamma_corrected.r * 255.f, gamma_corrected.g * 255.f, gamma_corrected.b * 255.f));
                }
            }
        }
        else
        {
            for (int x = 0; x < ScreenWidth(); x++)
            {
                for (int y = 0; y < ScreenHeight(); y++)
                {
                    const auto averaged_color = frame_buffer[x + y * ScreenWidth()] / static_cast<Real>(accumulated_frames);

                    // Reinhard filter https://en.wikipedia.org/wiki/Tone_mapping
                    const auto tone_mapped = averaged_color / (averaged_color + glm::vec3{ 1.f });
                    // Gamma correction, 2.2 common for sRGB https://en.wikipedia.org/wiki/Gamma_correction
                    const auto gamma_corrected = glm::pow(tone_mapped, glm::vec3{ 1.f / 2.2f });

                    Draw(x, y, olc::Pixel(gamma_corrected.r * 255.f, gamma_corrected.g * 255.f, gamma_corrected.b * 255.f));
                }
            }
        }

        frame_history.push(std::vector<glm::vec3>(staging_buffer, staging_buffer + ScreenWidth() * ScreenHeight()));

        last_mouse_position = GetMousePos();
        last_dirty = dirty;
        dirty = false;
        accumulated_frames++;

		return true;
	}

    bool OnUserDestroy() override
    {
        return true;
    }
};

// (c) Connor J. Link. Attribution from personal work outside of ISU.
// Utility function that does not meaningfully affect project functionality.
struct ParseResult
{
	bool success;
	int result;
};
ParseResult parse_int(const std::string& input)
{
	int result = 0;
	const char* begin = input.data();
	const char* end = begin + input.size();
	auto [ptr, ec] = std::from_chars(begin, end, result);
	bool success = (ec == std::errc() && ptr == end);
	return { success, result };
}

int main(int argc, char** argv)
{
    int width = 500, height = 500;

    if (argc > 1)
    {
        for (auto i = 1uz; i < argc; i++) 
        {
            const auto argument = std::string(argv[i]);
            const auto name = argument.substr(0, argument.find('='));
            const auto value = argument.substr(argument.find('=') + 1);

            if (name == "-width")
            {
                const auto result = parse_int(value);
                if (result.success)
                {
                    width = result.result;
                }
            }
            else if (name == "-height")
            {
                const auto result = parse_int(value);
                if (result.success)
                {
                    height = result.result;
                }
            }
            else if (name == "-bounces")
            {
                const auto result = parse_int(value);
                if (result.success)
                {
                    _bounces = result.result;
                }
            }
            else if (name == "-samples")
            {
                const auto result = parse_int(value);
                if (result.success)
                {
                    _samples = result.result;
                }
            }
            else if (name == "-captures")
            {
                const auto result = parse_int(value);
                if (result.success)
                {
                    _captures = result.result;
                }
            }
        }
    }

	Irradiance application{};
	if (application.Construct(width, height, 3, 3) == olc::OK)
    {
		application.Start();
    }

	return 0;
}
