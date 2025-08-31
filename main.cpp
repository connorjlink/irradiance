// to silence intellisense errors
#define _LIBCPP_ENABLE_EXPERIMENTAL

#define STBI_NEON
#define OLC_PGE_APPLICATION
#define OLC_IMAGE_STB
#include "olcPixelGameEngine.h"

#include <charconv>
#include <string>
#include <algorithm>
#include <numeric>
#include <execution>
#include <memory>

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_NEON
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/random.hpp"
#include "glm/gtx/norm.hpp"

#include "utility.h"
#include "renderer.h"
#include "scenes.h"
#include "meshes.h"

// main.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

using namespace ir;

static constexpr Real MOUSE_SENSITIVITY = 20.f;
static constexpr Real MOVEMENT_SPEED = 5.f;
static const glm::vec3 UP = glm::vec3{ 0.f, 1.f, 0.f };
static constexpr Real SAMPLE_JITTER = .001f;

static constexpr Real NONMETAL_REFLECTANCE = .04f;

static constexpr int FRAME_HISTORY = 5;

static constexpr bool ENABLE_SKYBOX = true;

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
    olc::vi2d last_mouse_position = { 0, 0 };
    bool dirty = true;
    bool last_dirty = false;
    glm::vec3 position = { 0.f, 0.f, -0.99f };
    Real fov_degrees = 90.f;
    Real yaw_degrees = 0.f;
    Real pitch_degrees = 0.f;
    int accumulated_frames = 1;
    Ray* rays = nullptr;
    bool enable_dof = false;
    Real focal_distance = std::numeric_limits<Real>::infinity();
    Real aperture_radius = .1f;

    glm::vec3* frame_buffer = nullptr;
    glm::vec3* staging_buffer = nullptr;

    CircularBuffer<std::vector<glm::vec3>, FRAME_HISTORY> frame_history;

    std::vector<int> index_buffer;

public:
    struct Emitter
    {
        Object* object = nullptr;
        Real power = 0.f;
        Real probability = 0.f;
    };

    std::vector<Object*> scene_objects;
    std::vector<Emitter> emissive_objects;

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

    glm::vec2 compute_skybox_uv_coordinates(const glm::vec3& direction) const
    {
        const auto theta = glm::atan(direction.z, direction.x);
        const auto phi = glm::acos(-direction.y); 

        auto u = (theta + glm::pi<Real>()) / (2.f * glm::pi<Real>());
        auto v = phi / glm::pi<Real>();

        u = 1.f - u;

        return { u, v };
    }

    glm::vec3 trace(Ray& ray, int bounces)
    {
        if (bounces <= 0)
        {
            return glm::vec3{ 0.f };
        }

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
            // NOTE: evidently cannot draw from the parallelized loop: gets malloc_break seg-faults
            //DrawRectDecal({ 1.f, 4.f }, { 2.f, 2.f }, olc::BLUE);

            // for testing only
            //std::cout << "Hit at depth: " << intersection.depth << "\n";

            if (nearest_intersection.material.emission != glm::vec3{ 0.f })
            {
                // emissive surfaces terminate bouncing
                return nearest_intersection.material.emission;
            }

            auto albedo = nearest_intersection.material.albedo;

            if (nearest_intersection.material.texture)
            {
                const auto& uv = nearest_intersection.uv;
                const auto sample = nearest_intersection.material.texture->Sample(uv.x, uv.y, nearest_intersection.position);
                albedo = glm::vec3{ sample.r / 255.f, sample.g / 255.f, sample.b / 255.f };
            }
                
            // ensure normal is relative to the front face
            auto normal = nearest_intersection.normal;
            if (glm::dot(normal, ray.direction) > 0.f)
            {
                normal = -normal;
            }
            const auto reflection = glm::reflect(ray.direction, normal);

            // ensure random sample hits hemisphere above the front face surface normal
            auto random_in_unit_sphere = glm::sphericalRand(1.f);
            if (glm::dot(random_in_unit_sphere, normal) < 0.f)
            {
                random_in_unit_sphere = -random_in_unit_sphere;
            }

            // move to next bounce
            ray.origin = nearest_intersection.position + normal * .001f; // offset to prevent self-intersection

            // TODO: incorporate importance sampling / probabilistic reflection

            auto emissivity = [](auto* object)
            {
                return object->area * object->material.emission.length();
            };

            Object* sampled_light = nullptr;
            for (const auto& light : emissive_objects)
            {
                // TODO: better sampling strategy than linear

                if (glm::linearRand(0.f, 1.f) < light.probability)
                {
                    sampled_light = light.object;
                    break;
                }
            }

            const auto light_direction = sampled_light 
                ? glm::normalize(sampled_light->centroid - ray.origin)
                : glm::vec3{ 0.f };

            const auto light_angle = glm::clamp(glm::dot(normal, light_direction), 0.f, 1.f);
            const auto normal_angle = glm::clamp(glm::dot(-ray.direction, normal), 0.f, 1.f);

            // TODO: maybe need to do shadow occlusion testing??

            const auto radiance = sampled_light ? glm::length(sampled_light->material.emission) : 0.f;

            auto brdf = [&]()
            {
                // Fresnel term with Schlick's approximation
                const auto F0 = glm::mix(glm::vec3{ NONMETAL_REFLECTANCE }, albedo, nearest_intersection.material.metallicity);
                // negative direction for non-incident ray
                const auto view_angle = glm::clamp(glm::dot(-ray.direction, reflection), 0.f, 1.f);
                const auto F = F0 + (glm::vec3{ 1.f } - F0) * glm::pow(1.f - normal_angle, 5.f); 
                // Lambertian diffuse https://en.wikipedia.org/wiki/Lambertian_reflectance

                const auto D = 1.f;

                const auto G = 1.f;

                // IMPORTANT: was getting division by zero issues without the added epsilon!!!!
                return (F * D * G) / (4.f * normal_angle * view_angle + .001f);
            };

            const auto base = brdf();
            const auto geometry = (light_angle * normal_angle) / glm::distance2(sampled_light->centroid, ray.origin);
            const auto probability = sampled_light ? 1.f / sampled_light->area : 1.f;

            // next-event estimation visibility checking per bounce
            // https://www.cg.tuwien.ac.at/sites/default/files/course/4411/attachments/08_next%20event%20estimation.pdf
            auto occlusion = 1.f;
            for (const auto& object : scene_objects)
            {
                if (object == sampled_light) 
                {
                    continue;
                }

                const auto intersection = object->intersect(ray);
                if (intersection.hit && intersection.depth < glm::length(sampled_light->centroid - ray.origin))
                {
                    occlusion = 0.f;
                    break;
                }
            }

            const auto direct = (base * radiance * geometry * occlusion) / probability;

            ray.direction = glm::normalize(reflection + random_in_unit_sphere * nearest_intersection.material.roughness);
            const auto indirect = trace(ray, bounces - 1);

            return direct + indirect;
        }
        else
        {
            if constexpr (ENABLE_SKYBOX)
            {
                const auto uv = compute_skybox_uv_coordinates(ray.direction);
                const auto sample = skybox->Sample(uv.x, uv.y);
                return glm::vec3{ sample.r / 255.f, sample.g / 255.f, sample.b / 255.f };
            }
        }

        return glm::vec3{ 0.f };
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

        // precompute an index sequence to be used for the parallelized for_each render loop
        index_buffer.resize(number, 0);
        std::iota(index_buffer.begin(), index_buffer.end(), 0);

        initialize_textures();
        //scene_objects = test_spheres();
        scene_objects = cornell_box();

        scene_objects.emplace_back(new Sphere
        { 
            glm::vec3{ .5f, .6f, .5f }, 
            .4f, 
            PBRMaterial
            {
                .albedo = glm::vec3{ .9f, .9f, .9f },
                .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                .emission = glm::vec3{ 0.f, 0.f, 0.f },
                .metallicity = .9f,
                .anisotropy = 0.f,
                .roughness = 0.f,
            }
        });

        const auto mesh = cube(glm::translate(glm::rotate(glm::scale(glm::identity<glm::mat4>(), glm::vec3{ .2f }), glm::radians(25.f), UP), glm::vec3{ -.5f, 0.f, -.5f }), PBRMaterial
        {
            .albedo = glm::vec3{ .9f, .9f, .9f },
            .absorption = glm::vec3{ 0.f, 0.f, 0.f },
            .emission = glm::vec3{ 0.f, 0.f, 0.f },
            .metallicity = 0.f,
            .anisotropy = 0.f,
            .roughness = .5f,
        });
        scene_objects.insert(scene_objects.end(), mesh.begin(), mesh.end());

        //scene_objects.insert(scene_objects.end(), icosphere.begin(), icosphere.end());
        //scene_objects.insert(scene_objects.end(), torus.begin(), torus.end());
        //scene_objects.insert(scene_objects.end(), cylinder.begin(), cylinder.end());
        //scene_objects.insert(scene_objects.end(), teapot.begin(), teapot.end());

        for (auto& object : scene_objects)
        {
            if (object && object->material.emission != glm::vec3{ 0.f })
            {
                emissive_objects.emplace_back(Emitter
                { 
                    .object = object, 
                    .power = glm::length(object->material.emission), 
                    .probability = 0.f,
                });
            }
        }

        auto emissivity = [](auto emitter)
        {
            return emitter.object->area * glm::length(emitter.object->material.emission);
        };

        for (auto& emitter : emissive_objects)
        {
            // pre-compute the probability of sampling each emitter weighted by emissivity as part of importance sampling
            emitter.probability = emissivity(emitter) / std::accumulate(emissive_objects.begin(), emissive_objects.end(), 0.f, 
                [&](auto sum, auto emitter) { return sum + emissivity(emitter); });
        }

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
        DrawStringPropDecal({ 5.f, 5.f }, std::format("Frames: {}, ms/frame: {:.2f}", accumulated_frames, fElapsedTime * 1000.f), olc::YELLOW);
        DrawStringPropDecal({ 5.f, 15.f }, std::format("Position: ({:.2f}, {:.2f}, {:.2f})", position.x, position.y, position.z), olc::YELLOW);
        DrawStringPropDecal({ 5.f, 25.f }, std::format("Yaw: {:.2f} Pitch: {:.2f}", yaw_degrees, pitch_degrees), olc::YELLOW);
        DrawStringPropDecal({ 5.f, 35.f }, std::format("DOF: {} @ {:.2f}", enable_dof ? "ON" : "OFF", focal_distance), olc::YELLOW);
        DrawStringPropDecal({ 5.f, 55.f }, std::format("Aperture Size: {:.2f}", aperture_radius), olc::YELLOW);
        DrawStringPropDecal({ 5.f, 65.f }, std::format("FOV: {:.2f}", fov_degrees), olc::YELLOW);

        const auto& ray = rays[GetMouseX() + GetMouseY() * ScreenWidth()].direction;
        DrawStringPropDecal({ 5.f, 45.f }, std::format("Ray: ({:2f}, {:2f}, {:2f})", ray.x, ray.y, ray.z), olc::YELLOW);

        if (GetMouse(olc::Mouse::LEFT).bHeld || GetMouse(olc::Mouse::RIGHT).bHeld)
        {
            const auto delta = GetMousePos() - last_mouse_position;

            yaw_degrees -= static_cast<Real>(delta.x) * fElapsedTime * MOUSE_SENSITIVITY;

            pitch_degrees += static_cast<Real>(delta.y) * fElapsedTime * MOUSE_SENSITIVITY;
            // prevents gimbal lock because the direction uses Euler angles
            pitch_degrees = glm::clamp(pitch_degrees, -80.f, 80.f);

            last_mouse_position = GetMousePos();

            dirty = true;
        }

        // adjust the DOF focal distance by clicking anywhere in the scene
        if (GetMouse(olc::Mouse::MIDDLE).bPressed || GetKey(olc::Key::F).bPressed)
        {
            const auto pixel = GetMouseX() + GetMouseY() * ScreenWidth();
            // explicit copy to avoid threading issues with the main render loop
            auto ray = rays[pixel];

            // TODO: incorporate with new bounding volume accelerator

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
                focal_distance = nearest_intersection.depth;
            }
            else
            {
                // clicked on the skybox: "infinitely" far away
                focal_distance = std::numeric_limits<Real>::infinity();
            }

            dirty = true;
        }
        // or adjust by scrolling the wheel
        const auto wheel = GetMouseWheel();
        if (wheel != 0)
        {
            focal_distance += static_cast<Real>(wheel) * fElapsedTime;
            // small epsilon required--0.f crashes the program from diskRand()!
            focal_distance = glm::max(focal_distance, .001f);
            dirty = true;
        }

        auto movement_speed = MOVEMENT_SPEED * (GetKey(olc::Key::SHIFT).bHeld ? 10.f : 1.f);

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
            enable_dof = !enable_dof;
            dirty = true;
        }
        if (GetKey(olc::Key::UP).bHeld)
        {
            aperture_radius += fElapsedTime * .5f;
            // small epsilon required--0.f crashes the program from diskRand()!
            aperture_radius = glm::max(aperture_radius, .001f);
            dirty = true;
        }
        if (GetKey(olc::Key::DOWN).bHeld)
        {
            aperture_radius -= fElapsedTime * .5f;
            // small epsilon required--0.f crashes the program from diskRand()!
            aperture_radius = glm::max(aperture_radius, .001f);
            dirty = true;
        }

        if (GetKey(olc::Key::PGUP).bPressed || GetKey(olc::Key::X).bPressed)
        {
            fov_degrees *= .9f;
            fov_degrees = glm::clamp(fov_degrees, 10.f, 170.f);
            dirty = true;
        }
        if (GetKey(olc::Key::PGDN).bPressed || GetKey(olc::Key::Z).bPressed)
        {
            fov_degrees /= .9f;
            fov_degrees = glm::clamp(fov_degrees, 10.f, 170.f);
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
                ray_jittered.direction = glm::normalize(ray_jittered.direction);

                if (enable_dof)
                {
                    const auto disk_sample = glm::diskRand(aperture_radius);
                    // effectively runs the UV coordinate-back calculation like in https://raytracing.github.io/books/RayTracingInOneWeekend.html#dielectrics/refraction
                    ray_jittered.origin += compute_right() * disk_sample.x + UP * disk_sample.y;
                    // TODO: ask Schaeffer about this step. It works correctly per Ray Tracing in One Weekend, but not sure why.
                    const auto focal_point = ray.origin + ray.direction * focal_distance;
                    ray_jittered.direction = glm::normalize(focal_point - ray_jittered.origin);
                }

                total_color += trace(ray_jittered, _bounces);
            }

            // smart NaN check per https://raytracing.github.io/books/RayTracingTheRestOfYourLife.html#playingwithimportancesampling
            if (total_color != total_color) 
            {
                total_color = glm::vec3{ 0.f };
            } 

            total_color /= static_cast<Real>(_samples);

            // Reinhard filter https://en.wikipedia.org/wiki/Tone_mapping
            const auto tone_mapped = total_color / (total_color + glm::vec3{ 1.f });
            // Gamma correction, 2.2 common for sRGB https://en.wikipedia.org/wiki/Gamma_correction
            const auto gamma_corrected = glm::pow(tone_mapped, glm::vec3{ 1.f / 2.2f });

            staging_buffer[i] = gamma_corrected;
            frame_buffer[i] += gamma_corrected;
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
                    Draw(x, y, olc::Pixel(color.r * 255.f, color.g * 255.f, color.b * 255.f));
                }
            }
        }
        else
        {
            for (int x = 0; x < ScreenWidth(); x++)
            {
                for (int y = 0; y < ScreenHeight(); y++)
                {
                    const auto color = frame_buffer[x + y * ScreenWidth()] / static_cast<Real>(accumulated_frames);
                    Draw(x, y, olc::Pixel(color.r * 255.f, color.g * 255.f, color.b * 255.f));
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
	if (application.Construct(width, height, 3, 3, false, false, false, false) == olc::OK)
    {
		application.Start();
    }

	return 0;
}
