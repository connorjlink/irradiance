// to silence intellisense errors
#define _LIBCPP_ENABLE_EXPERIMENTAL

#define OLC_PGE_APPLICATION
#define OLC_IMAGE_STB
#include "olcPixelGameEngine.h"
#define STBI_NEON
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <charconv>
#include <string>
#include <algorithm>
#include <numeric>
#include <execution>
#include <memory>
#include <deque>

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_NEON
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/random.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/norm.hpp"

#include "utility.h"
#include "renderer.h"
#include "scenes.h"
#include "cache.h"
#include "meshes.h"
#include "transform.h"

// main.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

using namespace ir;

static constexpr Real MOUSE_SENSITIVITY = 20.f;
static constexpr Real MOVEMENT_SPEED = 5.f;
static const glm::vec3 UP = glm::vec3{ 0.f, 1.f, 0.f };
static constexpr Real SAMPLE_JITTER = .001f;

static constexpr Real NONMETAL_REFLECTANCE = .04f;

static constexpr Real BASE_ISO = 25.f;
static constexpr Real REFERENCE_ISO = 4.f * 4.f * BASE_ISO; // ISO400
static constexpr Real MAX_ISO_MULTIPLIER = 128.f;

static constexpr Real SENSOR_HEIGHT = 35.f; // full-frame sensor mm

static constexpr Real HISTORY_HALF_LIFE_STABLE =  .25f;
static constexpr Real HISTORY_HALF_LIFE_DIRTY  = .1f;
static constexpr Real HISTORY_EPSILON = 1e-6f;

static constexpr Real CLEAN_EXPIRY = 1.f;

static constexpr int FRAME_HISTORY = 1;

#ifndef CORNELL
    static constexpr bool ENABLE_SKYBOX = true;
#else
    static constexpr bool ENABLE_SKYBOX = false;
#endif


int _bounces = 2;
int _samples = 5;

class Irradiance : public olc::PixelGameEngine
{
public:
	Irradiance()
	{
		sAppName = "Irradiance";
	}

private:
    static constexpr std::array<Real, 10> SHUTTER_SPEEDS
    {
        1 / 1000.f,
        1 / 500.f,
        1 / 250.f,
        1 / 125.f,
        1 / 60.f,
        1 / 30.f,
        1 / 15.f,
        1 / 10.f,
        1 / 5.f,
        1 / 2.f
    };

private:
    olc::vi2d last_mouse_position = { 0, 0 };
    bool dirty = true;
    bool last_dirty = false;
    Real clean_timer = 0.f;
    glm::vec3 position = { 0.95f, -.5f, -0.95f };
    Real fov_degrees = 90.f;
    Real yaw_degrees = -45.f, pitch_degrees = 20.f;
    int accumulated_frames = 1;
    Ray* rays = nullptr;
    bool enable_dof = false;
    Real focal_distance = std::numeric_limits<Real>::infinity();
    Real aperture_radius = .32f;
    Real ISO = REFERENCE_ISO;

    Real shutter_speed;
    std::size_t shutter_index = 5;
    bool enable_ui = true;
    bool tonemap_old = false;

    glm::vec3* frame_buffer = nullptr;
    std::vector<int> index_buffer;
    std::vector<glm::vec2> xy_buffer;
    std::vector<glm::vec2> uv_buffer;
    std::vector<glm::vec2> ndc_buffer;
    glm::vec2 reciprocal_dimensions;

private:
    struct Reprojection
    {
        Ray ray;
        glm::vec3 position;
        glm::vec3 normal;
        Real depth;
        glm::vec3 color;
        bool hit;
    };

    std::vector<Reprojection> reprojection_buffer;
    std::vector<Real> sample_counts;

private:
    RadianceCache cache;
    std::size_t cache_hits = 0;
    std::size_t cache_queries = 1;
    Real statistics_timer = 0.f;

private:
    std::size_t reprojection_hits = 0;
    std::size_t reprojection_queries = 1;
    Diffractor diffractor{};

public:
    struct Emitter
    {
        Object* object = nullptr;
        Real power = 0.f;
        Real probability = 0.f;
    };

    std::vector<MeshInstance*> scene_instances;
    TLAS* tlas = nullptr;
    std::vector<Emitter> emissive_objects;

public:
    struct ShutterSample
    {
        // camera orientation
        glm::vec3 position;
        Real yaw;
        Real pitch;
        Real time;
    };

    Real now = 0.f;
    std::deque<ShutterSample> shutter_history;

public:
    void capture_shutter(Real timestamp)
    {
        shutter_history.push_back(ShutterSample{ position, yaw_degrees, pitch_degrees, timestamp });
        
        // maintain shutter history within shutter speed interval only
        const auto discard_time = timestamp - shutter_speed;
        while (shutter_history.size() > 2 && shutter_history.front().time < discard_time)
        {
            shutter_history.pop_front();
        }
    }

    ShutterSample sample_shutter(Real timestamp)
    {
        if (shutter_history.size() < 2)
        {
            return ShutterSample{ position, yaw_degrees, pitch_degrees, timestamp };
        }

        // find the two samples surrounding the target time and lerp

        auto next = shutter_history.begin();
        auto previous = next++;

        while (next != shutter_history.end() && next->time < timestamp)
        {
            previous = next++;
        }

        if (next == shutter_history.end())
        {
            return *previous;
        }

        const auto interval = next->time - previous->time;
        const auto scalar = (timestamp - previous->time) / interval;

        // NOTE: slerp didn't seem to have any discernable improvement
        //const auto previous_quaternion = glm::quat{ glm::radians(glm::vec3{ previous->pitch, previous->yaw, 0.f }) };
        //const auto next_quaternion = glm::quat{ glm::radians(glm::vec3{ next->pitch, next->yaw, 0.f }) };
        //const auto interpolated_quaternion = glm::slerp(previous_quaternion, next_quaternion, scalar);
        //const auto interpolated_euler_angles = glm::degrees(glm::eulerAngles(interpolated_quaternion));

        return ShutterSample
        {
            .position = glm::mix(previous->position, next->position, scalar),
            .yaw = glm::mix(previous->yaw, next->yaw, scalar),
            .pitch = glm::mix(previous->pitch, next->pitch, scalar),
            .time = timestamp
        };
    }

    glm::vec3 tonemap_aces(const glm::vec3& color)
    {
        // excellent approximation of proper matrix ACES
        // full credit to Krzysztof Narkowicz: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
    
        const auto a = 2.51f;
        const auto b = 0.03f;
        const auto c = 2.43f;
        const auto d = 0.59f;
        const auto e = 0.14f;

        return (color * (a * color + b)) / (color * (c * color + d) + e);
    }

    glm::vec3 tonemap_reinhard(const glm::vec3& color)
    {
        // Reinhard filter https://en.wikipedia.org/wiki/Tone_mapping

        return color / (color + glm::vec3{ 1.f });
    }

    glm::vec3 tonemap(const glm::vec3& color)
    {
        if (!tonemap_old)
        {
            return tonemap_aces(color);
        }
        else
        {
            return tonemap_reinhard(color);
        }
    }

    glm::vec3 gamma_correct(const glm::vec3& color)
    {
        // Gamma correction, 2.2 common for sRGB https://en.wikipedia.org/wiki/Gamma_correction

        const auto corrected = glm::pow(color, glm::vec3{ 1.f });
        return glm::clamp(corrected, 0.f, 1.f);
    }

    std::string capture_screenshot()
    {
        const auto now = std::chrono::system_clock::now();
        const auto filepath = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count()) + ".png";

        struct RGB
        {
            std::uint8_t R, G, B;
        };

        auto rgb = new RGB[ScreenWidth() * ScreenHeight()];

        for (auto y = 0; y < ScreenHeight(); y++)
        {
            for (auto x = 0; x < ScreenWidth(); x++)
            {
                const auto index = x + y * ScreenWidth();

                const auto original = frame_buffer[x + y * ScreenWidth()] / sample_counts[index];

                const auto gamma_corrected = gamma_correct(original);

                const auto color = RGB
                { 
                    static_cast<std::uint8_t>(255.f * gamma_corrected.r), 
                    static_cast<std::uint8_t>(255.f * gamma_corrected.g), 
                    static_cast<std::uint8_t>(255.f * gamma_corrected.b)
                };

                rgb[index] = color;
            }
        }
        
        stbi_write_png(filepath.c_str(), ScreenWidth(), ScreenHeight(), 3, rgb, ScreenWidth() * sizeof(RGB));
        
        delete[] rgb;

        return filepath;
    }

    RayIntersection compute_nearest_intersection(const Ray& ray)
    {
        if (tlas)
        {
            return tlas->intersect(ray);
        }
        else
        {
            auto nearest_intersection = RayIntersection{};

            for (const auto& instance : scene_instances)
            {
                const auto intersection = instance->intersect(ray);

                if (intersection.hit && intersection.depth < nearest_intersection.depth)
                {
                    nearest_intersection = intersection;
                }
            }

            return nearest_intersection;
        }
    }

    glm::vec3 compute_direction(Real yaw, Real pitch) const
    {
        const auto yaw_radians = glm::radians(yaw);
        const auto pitch_radians = glm::radians(pitch);

        const auto x = glm::cos(pitch_radians) * glm::sin(yaw_radians);
        const auto y = glm::sin(pitch_radians);
        const auto z = glm::cos(pitch_radians) * glm::cos(yaw_radians);

        return glm::normalize(glm::vec3{ x, y, z });
    }

    glm::vec3 compute_right(Real yaw, Real pitch) const
    {
        const auto direction = compute_direction(yaw, pitch);
        return glm::normalize(glm::cross(direction, UP));
    }

    glm::vec2   compute_skybox_uv_coordinates(const glm::vec3& direction) const
    {
        const auto theta = glm::atan(direction.z, direction.x);
        const auto phi = glm::acos(-direction.y); 

        auto u = (theta + glm::pi<Real>()) / (2.f * glm::pi<Real>());
        auto v = phi / glm::pi<Real>();

        u = 1.f - u;

        return { u, v };
    }

    Real compute_emissivity(const Emitter& emitter)
    {
        return emitter.object->area * glm::length(emitter.object->material.emission);
    };

    #define INTERNAL_REVALIDATE(x, y) do { if (glm::isinf(x) || glm::isnan(x)) { x = y; } } while (0)
    #define REVALIDATE(x) INTERNAL_REVALIDATE(x, 0.f)

    Real compute_GGX_D(const glm::vec3& half_vector, const glm::vec3& normal, Real roughness)
    {
        const auto roughness4 = roughness * roughness * roughness * roughness + .01f;

        const auto angle = glm::max(glm::dot(normal, half_vector), 0.f);
        const auto angle2 = angle * angle;

        const auto scalar = (angle2 * (roughness4 - 1.f) + 1.f);
        const auto denominator = glm::pi<Real>() * scalar * scalar;

        return roughness4 / denominator;
    }

    glm::vec3 compute_fresnel_F(const glm::vec3& F0, Real cosine)
    {
        // Schlick approximation
        return F0 + (1.f - F0) * glm::pow(1.f - cosine, 5.f);
    }

    Real compute_smith_G(const glm::vec3& light, const glm::vec3& view, const glm::vec3& normal, Real roughness)
    {
        //  https://schuttejoe.github.io/post/ggximportancesamplingpart2/

        const auto light_angle = glm::max(glm::dot(normal, light), 0.f);
        const auto view_angle = glm::max(glm::dot(normal, view), 0.f);

        const auto k = (roughness + 1.f) * (roughness + 1.f) / 8.f;

        const auto G1_light = light_angle / (light_angle * (1.f - k) + k);
        const auto G1_view = view_angle / (view_angle * (1.f - k) + k);

        return G1_light * G1_view;
    }

    glm::vec3 trace(Ray& ray, int bounces, RayIntersection& output_intersection)
    {
        if (bounces <= 0)
        {
            return glm::vec3{ 0.f };
        }

        const auto nearest_intersection = compute_nearest_intersection(ray);
        if (bounces == _bounces && nearest_intersection.hit)
        {
            output_intersection = nearest_intersection;
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
            const bool is_front_face = glm::dot(normal, ray.direction) < 0.f;
            normal = is_front_face ? normal : -normal;

            // ensure random sample hits hemisphere above the front face surface normal
            auto random_in_unit_sphere = glm::sphericalRand(1.f);
            if (glm::dot(random_in_unit_sphere, normal) < 0.f)
            {
                random_in_unit_sphere = -random_in_unit_sphere;
            }

            const auto& mat = nearest_intersection.material;

            const auto normal_angle = glm::clamp(glm::dot(normal, ray.direction), 0.f, 1.f);
            
            // Fresnel term with Schlick's approximation
            const auto F0 = glm::mix(glm::vec3{ NONMETAL_REFLECTANCE }, albedo, mat.metallicity);
            const auto F = compute_fresnel_F(F0, 1.f - normal_angle);

            const auto metal_probability = mat.metallicity;
            const auto reflection_probability = (1.f - mat.metallicity) * glm::compMax(F) + mat.metallicity;
            const auto refraction_probability = (1.f - mat.metallicity) * (1.f - glm::compMax(F)) * mat.transmission;
            const auto diffuse_probability = (1.f - mat.metallicity) * (1.f - glm::compMax(F)) * (1.f - mat.transmission);

            const auto total = metal_probability + reflection_probability + refraction_probability + diffuse_probability;

            const auto metal_weight = metal_probability / total;
            const auto reflection_weight = reflection_probability / total;
            const auto refraction_weight = refraction_probability / total;
            const auto diffuse_weight = diffuse_probability / total;

            const auto random = glm::linearRand(0.f, 1.f);

            auto absorption = glm::vec3{ 1.f };
            auto weight = 1.f;

            const auto reflection = glm::reflect(ray.direction, normal);

            auto shade_ggx = [&]()
            {
                const auto V = -ray.direction;
                const auto R = reflection; 
                const auto H = glm::normalize(R + V);

                const auto D = compute_GGX_D(H, normal, mat.roughness);
                const auto F = compute_fresnel_F(F0, glm::max(0.f, glm::dot(H, V)));
                const auto G = compute_smith_G(R, V, normal, mat.roughness);

                const auto reflection_angle = glm::max(0.f, glm::dot(normal, R));
                const auto view_angle = glm::max(0.f, glm::dot(normal, V));

                const auto ggx = (D * G * F) / (4.f * reflection_angle * view_angle);

                return ggx;
            };

            #define ENABLE_GGX_SPECULAR

            if (random < metal_weight)
            {
                // metallic reflection

                #ifdef ENABLE_GGX_SPECULAR

                const auto specular = shade_ggx();

                #else

                const auto specular = F;

                #endif
                
                ray.origin = nearest_intersection.position + normal * .001f;
                // FUTURE: sample according to roughness and anisotropy (need surface anisotropy tangent basis, e.g., metal grain)
                ray.direction = glm::normalize(reflection + random_in_unit_sphere * nearest_intersection.material.roughness);
                
                absorption = specular * albedo;
                weight = metal_weight;
            }
            else if (random < metal_weight + reflection_weight)
            {
                // dielectric reflection

                #ifdef ENABLE_GGX_SPECULAR

                const auto specular = shade_ggx();
                
                #else

                const auto specular = F;

                #endif

                ray.origin = nearest_intersection.position + normal * .001f;
                // FUTURE: sample according to roughness and anisotropy (need surface anisotropy tangent basis, e.g., metal grain)
                ray.direction = glm::normalize(reflection + random_in_unit_sphere * nearest_intersection.material.roughness);

                absorption = specular * glm::vec3{ mat.transmission };
                weight = reflection_weight;
            }
            else if (random < metal_weight + reflection_weight + refraction_weight)
            {
                // dielectric refraction
                const auto eta = glm::dot(normal, -ray.direction) > 0.f 
                    ? (1.f / nearest_intersection.material.refraction_index) 
                    : nearest_intersection.material.refraction_index;

                auto refraction = glm::refract(ray.direction, normal, eta);

                if (glm::length2(refraction) < .001f)
                {
                    // total internal reflection
                    refraction = reflection;
                    ray.origin = nearest_intersection.position + normal * .001f;
                }
                else
                {
                    // NOTE: IMPORTANT--OFFSET IS POSSIBLY A NEGATIVE MARGIN TO AVOID SELF-INTERSECTION FOR REFRACTION RAY
                    ray.origin = nearest_intersection.position + (is_front_face ? -normal : normal) * .001f;
                }

                ray.direction = glm::normalize(refraction + random_in_unit_sphere * nearest_intersection.material.roughness);

                // Beer-Lambert attenuation (re-using albedo as absorption)
                const auto attenuation_distance = nearest_intersection.exit - nearest_intersection.depth;
                const auto attenuation = glm::exp(-mat.albedo * attenuation_distance);

                absorption = attenuation;
                weight = refraction_weight;
            }
            else
            {
                // diffuse scattering

                // cosine-weighted hemisphere random sampling per lambertian BRDF
                // heavily modified from the cosine distribution method plus re-basis using orthonormal space
                // https://www.rorydriscoll.com/2009/01/07/better-sampling/

                #define ENABLE_COSINE_SAMPLING
                #ifdef ENABLE_COSINE_SAMPLING

                auto disk = glm::diskRand(1.f);
                const auto z = glm::sqrt(glm::clamp(1.f - disk.x * disk.x - disk.y * disk.y, 0.f, 1.f));

                const auto local_coodinates = glm::vec3{ disk.x, disk.y, z };
            
                auto tangent = glm::normalize(glm::cross(normal, glm::vec3{ 0.f, 0.f, 1.f }));
                if (glm::length2(tangent) < .001f)
                {
                    tangent = glm::normalize(glm::cross(normal, glm::vec3{ 0.f, 1.f, 0.f }));
                }

                const auto bitangent = glm::normalize(glm::cross(tangent, normal));

                const auto basis = glm::mat3{ tangent, bitangent, normal };
                const auto world_coordinates = basis * local_coodinates;

                ray.origin = nearest_intersection.position + normal * .001f;
                ray.direction = glm::normalize(world_coordinates);

                #else

                ray.origin = nearest_intersection.position + normal * .001f;
                ray.direction = glm::normalize(reflection + random_in_unit_sphere);

                #endif

                absorption = albedo;
                weight = diffuse_weight;
            }

            auto path = glm::vec3{ 0.f };

            #define ENABLE_DLS
            #ifdef ENABLE_DLS

            // DIRECT LIGHT SAMPLING PATH TERMINATION

            if (!emissive_objects.empty())
            {
                auto sampled_emitter = Emitter{ nullptr, 0.f, 0.f }; 
                const auto emitter_random = glm::linearRand(0.f, 1.f);
                auto emitter_cdf = 0.f;
                for (const auto& emitter : emissive_objects)
                {
                    emitter_cdf += emitter.probability;
                    if (emitter_random <= emitter_cdf)
                    {
                        sampled_emitter = emitter;
                        break;
                    }
                }

                if (sampled_emitter.object)
                {
                    // direct light importance sampling https://raytracing.github.io/books/RayTracingTheRestOfYourLife.html#samplinglightsdirectly/
                    const auto light_sample = sampled_emitter.object->sample();
                    const auto light_normal = sampled_emitter.object->normal_of(light_sample);

                    auto light_direction = light_sample - ray.origin;
                    auto distance2 = glm::length2(light_direction);
                    light_direction = glm::normalize(light_direction);

                    const auto normal_cosine = glm::clamp(glm::dot(normal, light_direction), 0.f, 1.f);

                    const auto light_area = sampled_emitter.object->area;
                    const auto light_cosine = glm::clamp(glm::dot(light_normal, light_direction), 0.f, 1.f);

                    // next-event estimation direct light sampling per bounce
                    // https://www.cg.tuwien.ac.at/sites/default/files/course/4411/attachments/08_next%20event%20estimation.pdf
                    auto light_ray = Ray
                    {
                        .origin = ray.origin + normal * .001f,
                        .direction = light_direction,
                    };

                    const auto radiance = sampled_emitter.object->material.emission;

                    const auto pdf = distance2 / (light_cosine * light_area);

                    const auto occlusion = compute_nearest_intersection(light_ray);
                    if (occlusion.hit && occlusion.object == sampled_emitter.object)
                    {
                        const auto geometry = (normal_cosine * light_cosine) / distance2;
                        path += absorption * radiance * geometry / (weight * pdf);
                    }
                    else
                    {
                        if constexpr (ENABLE_SKYBOX)
                        {
                            const auto uv = compute_skybox_uv_coordinates(light_direction);
                            const auto sample = skybox->Sample(uv.x, uv.y);
                            const auto skybox_radiance = glm::vec3{ sample.r / 255.f, sample.g / 255.f, sample.b / 255.f };
                            const auto geometry = glm::clamp(glm::dot(normal, -reflection), 0.f, 1.f);
                            path += absorption * skybox_radiance * geometry / weight;
                        }
                    }
                }
            }
            
            #endif

            // STANDARD PATH TERMINATION
            {
                path += absorption * trace(ray, bounces - 1, output_intersection) / weight;
            }

            return path;
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

    Real compute_focal_length(Real fov)
    {
        return .5f * SENSOR_HEIGHT / glm::tan(glm::radians(fov) * .5f);
    }

    Real compute_fnumber(Real focal_length, Real aperture_radius)
    {
        return focal_length / (2.f * aperture_radius);
    }

public:
	bool OnUserCreate() override
	{
        const auto number = ScreenWidth() * ScreenHeight();
        
        rays = new Ray[number];
        frame_buffer = new glm::vec3[number];
        std::fill(frame_buffer, frame_buffer + number, glm::vec3{ 0.f });

        index_buffer.resize(number, 0);
        std::iota(index_buffer.begin(), index_buffer.end(), 0);

        xy_buffer.resize(number, { 0, 0 });
        for (auto i = 0; i < number; i++)
        {
            const auto x = static_cast<Real>(i % ScreenWidth());
            const auto y = static_cast<Real>(i / ScreenWidth());
            xy_buffer[i] = { x, y };
        }

        uv_buffer.resize(number, { 0, 0 });
        for (auto i = 0; i < number; i++)
        {
            const auto xy = xy_buffer[i];
            uv_buffer[i] = { xy.x / ScreenWidth(), xy.y / ScreenHeight() };
        }

        ndc_buffer.resize(number, { 0, 0 });
        for (auto i = 0; i < number; i++)
        {
            const auto uv = uv_buffer[i];
            ndc_buffer[i] = { 2.f * uv.x - 1.f, 2.f * uv.y - 1.f };
        }

        reprojection_buffer.resize(number, Reprojection{});
        sample_counts.resize(number, 1.f);

        reciprocal_dimensions = { 1.f / ScreenWidth(), 1.f / ScreenHeight() };

        diffractor = Diffractor{ static_cast<std::uint32_t>(ScreenWidth()), static_cast<std::uint32_t>(ScreenHeight()), 7, 0.f };

        shutter_speed = SHUTTER_SPEEDS[shutter_index];
        shutter_history = {};
        capture_shutter(now);

        initialize_textures();

    #ifndef CORNELL
        scene_instances.emplace_back(test_spheres());

        static const auto utah = teapot(PBRMaterial
        {
            .albedo = glm::vec3{ .9f, .2f, .9f },
            .emission = glm::vec3{ 0.f, 0.f, 0.f },
            .metallicity = .5f,
            .refraction_index = 1.5f,
            .anisotropy = 0.f,
            .roughness = .02f,
            .transmission = 0.f,
        });
        static const auto utah_instance = new MeshInstance
        {
            glm::rotate(glm::translate(glm::scale(glm::identity<glm::mat4>(), glm::vec3{ .5f }), glm::vec3{ 1.5f, -5.f, .5f }), glm::radians(-180.f), glm::vec3{ 1.f, 0.f, 0.f }),
            utah
        };
        scene_instances.emplace_back(utah_instance);

        // NOTE: for radiance cache testing only!!
        // scene_instances.emplace_back(new MeshInstance
        // {
        //     glm::identity<glm::mat4>(),
        //     Mesh
        //     {
        //         new Cuboid 
        //         {
        //             glm::vec3{ -20.f },
        //             glm::vec3{ 40.f, 40.f, 40.f },
        //             PBRMaterial
        //             {
        //                 .albedo = glm::vec3{ .9f, .0f, .0f },
        //                 .emission = glm::vec3{ 0.f, 0.f, 0.f },
        //                 .metallicity = 0.f,
        //                 .refraction_index = 1.5f,
        //                 .anisotropy = 0.f,
        //                 .roughness = .5f,
        //                 .transmission = .99f,
        //             }
        //         }
        //     }
        // });

    #else
        scene_instances.emplace_back(cornell_box());

        static const auto sphere = Mesh
        {
            new Sphere
            { 
                glm::vec3{ .5f, .6f, .5f }, 
                .4f, 
                PBRMaterial
                {
                    .albedo = glm::vec3{ .2f, .4f, .9f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = .1f,
                    .refraction_index = .1f,
                    .anisotropy = 0.f,
                    .roughness = 0.1f,
                    .transmission = .2f,
                }
            }
        };
        scene_instances.emplace_back(new MeshInstance{ glm::identity<glm::mat4>(), sphere });

        static const auto smoke = Mesh
        {
            new Colloid
            {
                2.f,
                new Sphere 
                {
                    glm::vec3{ .5f, -.25f, .5f }, 
                    .25f,
                    PBRMaterial
                    {
                        .albedo = glm::vec3{ 0.f, 0.f, 0.f },
                        .emission = glm::vec3{ 0.f, 0.f, 0.f },
                        .metallicity = 0.f,
                        .refraction_index = 1.5f,
                        .anisotropy = 0.f,
                        .roughness = 0.f,
                        .transmission = 0.f,
                    }
                }
            }
        };
        scene_instances.emplace_back(new MeshInstance{ glm::identity<glm::mat4>(), smoke });

        static const auto prism = Mesh
        {
            new Cuboid
            {
                glm::vec3{ -.5f, .0f, -.8f },
                glm::vec3{ .4f, .4f, .4f },
                PBRMaterial
                {
                    .albedo = glm::vec3{ .7f, 1.f, .8f },
                    .emission = glm::vec3{ 0.f, 0.f, 0.f },
                    .metallicity = 0.f,
                    .refraction_index = 2.1f,
                    .anisotropy = 0.f,
                    .roughness = .01f,
                    .transmission = .99f,
                }
            }
        };

        static const auto prism_instance = new MeshInstance
        {
            glm::identity<glm::mat4>(),
            prism
        };
        scene_instances.emplace_back(prism_instance);

        static const auto suzanne = monkey(PBRMaterial
        {
            .albedo = glm::vec3{ .7f, .2f, .9f },
            .emission = glm::vec3{ 0.f, 0.f, 0.f },
            .metallicity = .7f,
            .refraction_index = 1.5f,
            .anisotropy = 0.f,
            .roughness = .5f,
            .transmission = .1f,
        });
        static const auto monkey_instance = new MeshInstance
        {
            glm::rotate(glm::translate(glm::scale(glm::identity<glm::mat4>(), glm::vec3{ .55f }), glm::vec3{ -.75f, .75f, .75f }), glm::radians(-200.f), glm::vec3{ 1.f, .4f, 0.f }),
            suzanne
        };
        scene_instances.emplace_back(monkey_instance);

    #endif

        tlas = new TLAS{ scene_instances };

        auto world_minimum = glm::vec3{ std::numeric_limits<Real>::max() };
        auto world_maximum = glm::vec3{ std::numeric_limits<Real>::lowest() };

        for (auto instance : scene_instances)
        {
            world_minimum = glm::min(world_minimum, instance->volume->origin);
            world_maximum = glm::max(world_maximum, instance->volume->extent);

            for (auto object : instance->mesh)
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
        }

        cache = RadianceCache{ world_minimum, world_maximum, .5f };

        for (auto& emitter : emissive_objects)
        {
            // pre-compute the probability of sampling each emitter weighted by emissivity as part of importance sampling
            emitter.probability = compute_emissivity(emitter) / std::accumulate(emissive_objects.begin(), emissive_objects.end(), 0.f, 
                [&](auto sum, auto emitter) { return sum + compute_emissivity(emitter); });
        }

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
        if (enable_ui)
        {
            DrawStringPropDecal({ 5.f, 5.f }, std::format("Frames: {} ({:.2f} ms/frame)", accumulated_frames, fElapsedTime * 1000.f), olc::YELLOW);
            DrawStringPropDecal({ 5.f, 15.f }, std::format("Position: ({:.2f}, {:.2f}, {:.2f})", position.x, position.y, position.z), olc::YELLOW);
            DrawStringPropDecal({ 5.f, 25.f }, std::format("Yaw: {:.2f} Pitch: {:.2f}", yaw_degrees, pitch_degrees), olc::YELLOW);
            DrawStringPropDecal({ 5.f, 35.f }, std::format("Tonemapping: {}", tonemap_old ? "Reinhard" : "Approximated ACES"), olc::YELLOW);
            DrawStringPropDecal({ 5.f, 45.f }, std::format("DOF: {} @ {:.2f}", enable_dof ? "ON" : "OFF", focal_distance), olc::YELLOW);
            DrawStringPropDecal({ 5.f, 55.f }, std::format("Sensor: ISO {:.0f}, 1/{:.0f}s", ISO, 1.f / shutter_speed), olc::YELLOW);
            
            const auto focal_length = compute_focal_length(fov_degrees);
            const auto fnumber = compute_fnumber(focal_length, aperture_radius);
            DrawStringPropDecal({ 5.f, 65.f }, std::format("Focal Length: {:.2f}mm ({:.0f}deg)", focal_length, fov_degrees), olc::YELLOW);
            DrawStringPropDecal({ 5.f, 75.f }, std::format("Aperture: f/{:.2f} (r={:.2f}mm)", fnumber, aperture_radius), olc::YELLOW);
        }

        if (GetKey(olc::Key::P).bPressed)
        {
            const auto filepath = capture_screenshot();
            std::println("Screenshot captured at {}!", filepath);
        }

        if (GetKey(olc::Key::U).bPressed)
        {
            enable_ui = !enable_ui;
        }

        if (GetMouse(olc::Mouse::LEFT).bHeld || GetMouse(olc::Mouse::RIGHT).bHeld)
        {
            const auto delta = GetMousePos() - last_mouse_position;

            yaw_degrees -= static_cast<Real>(delta.x) * fElapsedTime * MOUSE_SENSITIVITY;

            pitch_degrees += static_cast<Real>(delta.y) * fElapsedTime * MOUSE_SENSITIVITY;
            // prevents gimbal lock because the direction uses Euler angles
            pitch_degrees = glm::clamp(pitch_degrees, -80.f, 80.f);

            last_mouse_position = GetMousePos();

            if (delta != olc::vi2d{ 0, 0 })
            {
                dirty = true;
            }
        }

        // adjust the DOF focal distance by clicking anywhere in the scene
        if (GetMouse(olc::Mouse::MIDDLE).bPressed || GetKey(olc::Key::F).bPressed)
        {
            const auto pixel = GetMouseX() + GetMouseY() * ScreenWidth();
            // explicit copy to avoid threading issues with the main render loop
            auto ray = rays[pixel];

            const auto nearest_intersection = compute_nearest_intersection(ray); 
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
            position += compute_direction(yaw_degrees, pitch_degrees) * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::S).bHeld)
        {
            position -= compute_direction(yaw_degrees, pitch_degrees) * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::A).bHeld)
        {
            const auto right = compute_right(yaw_degrees, pitch_degrees);
            position -= right * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::D).bHeld)
        {
            const auto right = compute_right(yaw_degrees, pitch_degrees);
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
        if (GetKey(olc::Key::UP).bPressed)
        {
            aperture_radius *= 2.f;
            dirty = true;
        }
        if (GetKey(olc::Key::DOWN).bPressed)
        {
            aperture_radius /= 2.f;
            dirty = true;
        }
        if (GetKey(olc::Key::HOME).bPressed || GetKey(olc::Key::RIGHT).bPressed)
        {
            ISO *= 2.f;
            ISO = glm::clamp(ISO, BASE_ISO, MAX_ISO_MULTIPLIER * REFERENCE_ISO);
            dirty = true;
        }
        if (GetKey(olc::Key::END).bPressed || GetKey(olc::Key::LEFT).bPressed)
        {
            ISO /= 2.f;
            ISO = glm::clamp(ISO, BASE_ISO, MAX_ISO_MULTIPLIER * REFERENCE_ISO);
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

        if (GetKey(olc::Key::K).bPressed)
        {
            shutter_index++;
            shutter_index = std::clamp(shutter_index, 0uz, SHUTTER_SPEEDS.size() - 1);
            shutter_speed = SHUTTER_SPEEDS[shutter_index];
            dirty = true;
        }
        if (GetKey(olc::Key::J).bPressed)
        {
            shutter_index--;
            shutter_index = std::clamp(shutter_index, 0uz, SHUTTER_SPEEDS.size() - 1);
            shutter_speed = SHUTTER_SPEEDS[shutter_index];
            dirty = true;
        }

        if (GetKey(olc::Key::Y).bPressed)
        {
            // diffract current framebuffer once

            diffractor.affect_diffraction(frame_buffer);

            for (auto i = 0; i < ScreenWidth() * ScreenHeight(); i++)
            {
                reprojection_buffer[i].color = frame_buffer[i];
            }
        }
            
        if (GetKey(olc::Key::T).bPressed)
        {
            tonemap_old = !tonemap_old;
            dirty = true;
        }

        statistics_timer += fElapsedTime;
        if (statistics_timer > 2.f)
        {
            statistics_timer = 0.f;

            std::println("Cache hit rate: {} / {} = {:.2f}%", cache_hits, cache_queries, cache_queries > 0 ? (static_cast<Real>(cache_hits) / static_cast<Real>(cache_queries)) * 100.f : 0.f);

            std::println("Reprojection hit rate: {} / {} = {:.2f}%", reprojection_hits, reprojection_queries, reprojection_queries > 0 ? (static_cast<Real>(reprojection_hits) / static_cast<Real>(reprojection_queries)) * 100.f : 0.f);   

            cache_hits = 0;
            cache_queries = 0;

            reprojection_hits = 0;
            reprojection_queries = 0;
        }

        now += fElapsedTime;
        capture_shutter(now);

        const auto aspect_ratio = static_cast<Real>(ScreenWidth()) / static_cast<Real>(ScreenHeight());
        const auto fov_radians = glm::radians(fov_degrees);

        const auto projection = glm::perspective(fov_radians, aspect_ratio, .1f, 1000.f);
        const auto inverse_projection = glm::inverse(projection);

        const auto current_direction = compute_direction(yaw_degrees, pitch_degrees);
        const auto current_view = glm::lookAt(position, position + current_direction, UP);
        const auto current_inverse_view = glm::inverse(current_view);
        
        auto uv_to_view_space = [&](Real ndc_x, Real ndc_y)
        {
            const auto clip_space = glm::vec4{ ndc_x, ndc_y, 1.f, 1.f };
            const auto homogenous_space = inverse_projection * clip_space;
            const auto world_space = homogenous_space / homogenous_space.w;

            return glm::vec4{ glm::normalize(glm::vec3{ world_space }), 0.f };
        };

        // PARALLELIZE
        
        std::for_each(std::execution::par, index_buffer.begin(), index_buffer.end(), [&](int i)
        {
            const auto x = xy_buffer[i].x;
            const auto y = xy_buffer[i].y;

            const auto ndc_x = ndc_buffer[i].x;
            const auto ndc_y = ndc_buffer[i].y;

            // correct the ray coordinate space for accurate focus click distance
            const auto view_space_direction = uv_to_view_space(ndc_x, ndc_y);
            const auto world_space_direction = current_inverse_view * view_space_direction;
            rays[i] = Ray{ position, glm::normalize(glm::vec3{ world_space_direction }), 0.f };

            auto total_color = glm::vec3{ 0.f };

            auto intersection = RayIntersection{};
            for (auto s = 0; s < _samples; s++)
            {
                // centered shutter interval over the current frame, interpolate according to frametime
                const auto timestamp = glm::linearRand(now - shutter_speed, now);
                const auto then = sample_shutter(timestamp);
                rays[i].timestamp = timestamp - now;

                // view_space = inverse_projection * homogenized NDC (clip_space)
                // world_space = inverse_view * view_space
                const auto interpolated_direction = compute_direction(then.yaw, then.pitch);
                const auto interpolated_view = glm::lookAt(then.position, then.position + interpolated_direction, UP);
                const auto interpolated_inverse_view = glm::inverse(interpolated_view);

                const auto interpolated_world_space_direction = interpolated_inverse_view * view_space_direction;
                const auto interpolated_ray = Ray
                {
                    then.position,
                    glm::normalize(glm::vec3{ interpolated_world_space_direction }),
                    timestamp
                };

                const auto u_jittered = (x + glm::linearRand(0.f, 1.f)) * reciprocal_dimensions.x;
                const auto v_jittered = (y + glm::linearRand(0.f, 1.f)) * reciprocal_dimensions.y;

                const auto ndc_x_jittered = 2.f * u_jittered - 1.f;
                const auto ndc_y_jittered = 2.f * v_jittered - 1.f;

                const auto interpolated_view_space_direction = uv_to_view_space(ndc_x_jittered, ndc_y_jittered);
                const auto interpolated_world_space_direction_jittered = interpolated_inverse_view * interpolated_view_space_direction;

                auto jittered_ray = Ray
                {
                    then.position,
                    glm::normalize(glm::vec3{ interpolated_world_space_direction_jittered }),
                    timestamp
                };

                if (enable_dof)
                {
                    const auto disk_sample = glm::diskRand(aperture_radius);
                    // effectively runs the UV coordinate-back calculation like in https://raytracing.github.io/books/RayTracingInOneWeekend.html#dielectrics/refraction
                    jittered_ray.origin += compute_right(then.yaw, then.pitch) * disk_sample.x + UP * disk_sample.y;
                    // NOTE: rays do not arrive to the camera parallel to the sensor, so must compute the corresponding focal point in world space first                    
                    const auto focal_point = interpolated_ray.origin + interpolated_ray.direction * focal_distance;
                    jittered_ray.direction = glm::normalize(focal_point - jittered_ray.origin);
                }

                auto result = trace(jittered_ray, _bounces, intersection);

                REVALIDATE(result.r);
                REVALIDATE(result.g);
                REVALIDATE(result.b);

                //#define ENABLE_RADIANCE_CACHE
                #ifdef ENABLE_RADIANCE_CACHE

                if (intersection.hit)
                {
                    total_queries++;

                    if (const auto cached = cache.query(intersection.position); cached.has_value())
                    {
                        total_color += cached->radiance;
                        cache_hits++;
                    }
                    else
                    {
                        total_color += result;

                        cache.insert(CacheEntry
                        {
                            .position = intersection.position,
                            .normal = intersection.normal,
                            .incidence = jittered_ray.direction,
                            .radiance = result,
                            .weight = 1.f / static_cast<Real>(_samples),
                            .last_used = now,
                        });
                    }
                }
                else
                {
                    total_color += result;
                }
                #else 
                
                total_color += result;

                #endif
            }

            total_color /= static_cast<Real>(_samples);
            // IMPORTANT: MUST APPLY ISO EXPOSURE CORRECTION BEFORE AVERAGING!!!!! OTHERWISE IT'S ALMOST GRAY
            const auto iso_corrected = total_color * (ISO / REFERENCE_ISO);
            const auto tone_mapped = tonemap(iso_corrected);
            
            const auto half_life = dirty ? HISTORY_HALF_LIFE_DIRTY : HISTORY_HALF_LIFE_STABLE;
            const auto decay = glm::pow(.5f, fElapsedTime / glm::max(half_life, HISTORY_EPSILON));

            if (clean_timer < CLEAN_EXPIRY)
            {
                sample_counts[i] = glm::max(sample_counts[i] * decay, HISTORY_EPSILON);
                frame_buffer[i] *= decay;
            }

            if (!GetKey(olc::Key::Q).bHeld)
            {
                sample_counts[i] += 1.f;
                frame_buffer[i] += tone_mapped;
            }

            #define ENABLE_REPROJECTION
            #ifdef ENABLE_REPROJECTION

            const auto previous_sample = reprojection_buffer[i];

            if (dirty)
            {
                const auto& previous_ray = previous_sample.ray;

                const auto world_space_position = glm::vec4{ previous_sample.position, 1.f };

                const auto view_space_reprojected = current_view * world_space_position;
                const auto homogenous_reprojected = projection * view_space_reprojected;
                const auto ndc_reprojected = homogenous_reprojected / homogenous_reprojected.w;
                const auto uv_reprojected = (glm::vec2{ ndc_reprojected } + 1.f) / 2.f;

                reprojection_queries++;
                if (uv_reprojected.x > 0.f && uv_reprojected.x < 1.f && 
                    uv_reprojected.y > 0.f && uv_reprojected.y < 1.f && 
                    previous_sample.hit)
                {
                    reprojection_hits++;

                    const auto point = uv_reprojected * glm::vec2{ ScreenWidth(), ScreenHeight() };

                    // 2x2 grid filter
                    const auto p0 = glm::floor(point);
                    const auto p1 = p0 + glm::vec2{ 1.f };

                    const auto f = point - p0;

                    // bilinear interpolation
                    const auto weights = glm::vec4
                    { 
                        (1.f - f.x) * (1.f - f.y),
                        f.x * (1.f - f.y),        
                        (1.f - f.x) * f.y,        
                        f.x * f.y                 
                    };

                    auto sample = [&](auto x, auto y, auto weight)
                    {
                        const auto j = static_cast<std::size_t>(x + y * ScreenWidth());
                        if (j < 0 || j >= sample_counts.size()) 
                        {
                            return;
                        }
                        sample_counts[j] += weight;
                        frame_buffer[j] += previous_sample.color * weight;
                    };

                    sample(p0.x, p0.y, weights.x);
                    sample(p1.x, p0.y, weights.y);
                    sample(p0.x, p1.y, weights.z);
                    sample(p1.x, p1.y, weights.w);
                }
            }

            const auto safe_color = (sample_counts[i] > 0.f)
                ? (frame_buffer[i] / sample_counts[i])
                : previous_sample.color;
            
            reprojection_buffer[i] = Reprojection
            {
                .ray = rays[i],
                .position = intersection.position,
                .normal = intersection.normal,
                .depth = intersection.depth,
                .color = safe_color,
                .hit = intersection.hit,
            };

            #endif
            
        });

        for (auto x = 0; x < ScreenWidth(); x++)
        {
            for (auto y = 0; y < ScreenHeight(); y++)
            {
                const auto index = x + y * ScreenWidth();
                const auto color = frame_buffer[index] / sample_counts[index];
                const auto gamma_corrected = gamma_correct(color);
                Draw(x, y, olc::Pixel(gamma_corrected.r * 255.f, gamma_corrected.g * 255.f, gamma_corrected.b * 255.f));
            }
        }

        if (!dirty && last_dirty && !GetKey(olc::Key::Q).bHeld)
        {
            const auto number = ScreenWidth() * ScreenHeight();
            std::fill(frame_buffer, frame_buffer + number, glm::vec3{ 0.f });
            std::fill(sample_counts.begin(), sample_counts.end(), 0.f);
            accumulated_frames = 1;
        }
        // TODO: necessary if view decay not used
        // if (!dirty && last_dirty)
        // {
        //     //std::fill(reprojection_buffer.begin(), reprojection_buffer.end(), Reprojection{});
        // }

        last_mouse_position = GetMousePos();

        clean_timer = dirty ? 0.f : clean_timer + fElapsedTime;

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

int main(int argc, char** argv)
{
    int width = 300, height = 300;

    auto hires = false;

    if (argc > 1)
    {
        for (auto i = 1uz; i < argc; i++) 
        {
            const auto argument = std::string(argv[i]);
            const auto name = argument.substr(0, argument.find('='));
            const auto value = argument.substr(argument.find('=') + 1);

            if (name == "-width")
            {
                const auto result = from_string<int>(value);
                if (result.success)
                {
                    width = result.result;
                }
            }
            else if (name == "-height")
            {
                const auto result = from_string<int>(value);
                if (result.success)
                {
                    height = result.result;
                }
            }
            else if (name == "-bounces")
            {
                const auto result = from_string<int>(value);
                if (result.success)
                {
                    _bounces = result.result;
                }
            }
            else if (name == "-samples")
            {
                const auto result = from_string<int>(value);
                if (result.success)
                {
                    _samples = result.result;
                }
            }
            else if (name == "-hires")
            {
                hires = true;
            }
        }
    }

    const auto PPP = hires ? 1 : 2;

	Irradiance application{};
	if (application.Construct(width, height, PPP, PPP, false, false, false, false) == olc::OK)
    {
		application.Start();
    }

	return 0;
}
