#ifndef IRRADIANCE_SHADER_H
#define IRRADIANCE_SHADER_H

// shader.h
// (c) 2025 Connor J. Link. All Rights Reserved.

#include "glm/glm.hpp"

#include "renderer.h"
#include "textures.h"

extern ir::TLAS* _tlas;
extern std::int32_t _bounces;
extern std::vector<ir::MeshInstance*> _scene_instances;

namespace ir
{
#if !defined(CORNELL) && !defined(CORNELL2)
    inline bool ENABLE_SKYBOX = true;
#else
    inline bool ENABLE_SKYBOX = false;
#endif

    static constexpr Real NONMETAL_REFLECTANCE = .04f;

    struct ShaderResult
    {
        glm::vec3 absorption;
        Ray outgoing;
        RayIntersection intersection = MISS;
    };

    glm::vec3 compute_base_reflectance(const PBRMaterial& material)
    {
        return glm::mix(glm::vec3{ NONMETAL_REFLECTANCE }, material.albedo, material.metallicity);
    }

    #define INTERNAL_REVALIDATE(x, y) do { if (glm::isinf(x) || glm::isnan(x)) { x = y; } } while (0)
    #define REVALIDATE(x) INTERNAL_REVALIDATE(x, 0.f)

    glm::vec3 trace(Ray&, RayIntersection&, std::int32_t);

    class Shader
    {
    public:
        static glm::vec3 random_in_unit_sphere(const glm::vec3& normal)
        {
            // ensure random sample hits hemisphere above the front face surface normal
            auto random_in_unit_sphere = glm::sphericalRand(1.f);
            if (glm::dot(random_in_unit_sphere, normal) < 0.f)
            {
                random_in_unit_sphere = -random_in_unit_sphere;
            }

            return random_in_unit_sphere;
        }
    
    private:
        // equivalent to BRDF in some cases
        virtual ShaderResult evaluate(const glm::vec3& incoming, const RayIntersection& intersection, std::int32_t bounces) const = 0;
        virtual Real pdf(const glm::vec3& incoming, const glm::vec3& outgoing, const RayIntersection& intersection) const = 0;

    public:
        ShaderResult sample(const glm::vec3& incoming, const RayIntersection& intersection, std::int32_t bounces) const
        {
            auto brdf = evaluate(incoming, intersection, bounces);
            brdf.absorption /= glm::max(pdf(incoming, brdf.outgoing.direction, brdf.intersection), EPSILON_F);
            return brdf;
        }
    };

    class EnvironmentShader : public Shader
    {
    private:
        ShaderResult evaluate(const glm::vec3& incoming, const RayIntersection& intersection, std::int32_t) const override
        {
            static const auto IGNORE = glm::vec3{ 0.f };

            auto absorption = glm::vec3{ 0.f };

            if (ENABLE_SKYBOX)
            {
                const auto uv = compute_skybox_uv_coordinates(incoming);
                const auto sample = skybox->Sample(uv.x, uv.y, IGNORE);
                absorption = glm::clamp(glm::vec3{ sample.r / 255.f, sample.g / 255.f, sample.b / 255.f }, 0.f, 1.f);
            }

            return ShaderResult
            {
                .absorption = absorption,
                .outgoing = { incoming },
                .intersection = MISS,
            };
        }

        Real pdf(const glm::vec3&, const glm::vec3&, const RayIntersection&) const override
        {
            // environment sampled deterministically, so MIS should not apply de-bias weighting
            return 1.f;
        }
    
    private:
        glm::vec2 compute_skybox_uv_coordinates(const glm::vec3& direction) const
        {
            const auto theta = glm::atan(direction.z, direction.x);
            const auto phi = glm::acos(-direction.y); 

            auto u = (theta + glm::pi<Real>()) / (2.f * glm::pi<Real>());
            auto v = phi / glm::pi<Real>();

            u = 1.f - u;

            return { u, v };
        }
    };

    class DiffuseShader : public Shader
    {
    private:
        ShaderResult evaluate(const glm::vec3&, const RayIntersection& intersection, std::int32_t) const override
        {
            // cosine-weighted hemisphere random sampling per lambertian BRDF
            // heavily modified from the cosine distribution method plus re-basis using orthonormal space
            // https://www.rorydriscoll.com/2009/01/07/better-sampling/

            #define ENABLE_COSINE_SAMPLING
            #ifdef ENABLE_COSINE_SAMPLING

            const auto& normal = intersection.normal;

            const auto disk = glm::diskRand(1.f);
            const auto z = glm::sqrt(glm::clamp(1.f - disk.x * disk.x - disk.y * disk.y, 0.f, 1.f));
            const auto local_coordinates = glm::vec3{ disk.x, disk.y, z };

            auto tangent = glm::normalize(glm::cross(normal, glm::vec3{ 0.f, 0.f, 1.f }));
            if (glm::length2(tangent) < EPSILON_F)
            {
                tangent = glm::normalize(glm::cross(normal, glm::vec3{ 0.f, 1.f, 0.f }));
            }

            const auto bitangent = glm::normalize(glm::cross(tangent, normal));

            const auto basis = glm::mat3{ tangent, bitangent, normal };
            const auto world_coordinates = glm::normalize(basis * local_coordinates);

            auto outgoing = Ray
            {
                .origin = intersection.position + normal * EPSILON_F,
                .direction = glm::normalize(world_coordinates),
            };

            #else

            const auto reflection = glm::reflect(ray.direction, intersection.normal);
            auto outgoing = Ray
            {
                .origin = intersection.position + normal * EPSILON_F,
                .direction = glm::normalize(reflection + random_in_unit_sphere),
            };

            #endif

            auto albedo = intersection.material.albedo;

            if (intersection.material.texture)
            {
                const auto& uv = intersection.uv;
                const auto tex = intersection.material.texture->Sample(uv.x, uv.y, intersection.position);
                albedo = glm::vec3{ tex.r, tex.g, tex.b } / 255.f;
            }

            const auto view_angle = glm::clamp(glm::dot(outgoing.direction, intersection.normal), 0.f, 1.f);
            const auto absorption = (albedo * view_angle) / glm::pi<Real>();

            return ShaderResult
            { 
                .absorption = absorption, 
                .outgoing = outgoing, 
                .intersection = intersection
            };
        }

        Real pdf(const glm::vec3& incoming, const glm::vec3& outgoing, const RayIntersection& intersection) const override
        {
            const auto normal_angle = glm::clamp(glm::dot(outgoing, intersection.normal), 0.f, 1.f);
            return normal_angle / glm::pi<Real>();
        }
    };
    
    #define ENABLE_GGX_SPECULAR

    class MetallicShader : public Shader
    {
    public:
        static glm::vec3 compute_fresnel_F(const glm::vec3& F0, Real cosine)
        {
            // Schlick approximation
            return F0 + (1.f - F0) * glm::pow(1.f - cosine, 5.f);
        }

    private:
        static glm::vec3 build_tangent(const glm::vec3& n)
        {
            static const auto IN = glm::vec3{ 0.f, 0.f, 1.f };
            static const auto UP = glm::vec3{ 0.f, 1.f, 0.f };

            auto tangent = glm::cross(n, IN);
            if (glm::length2(tangent) < EPSILON_F)
            {
                tangent = glm::cross(n, UP);
            }

            return glm::normalize(tangent);
        }

        static Real compute_GGX_D(const glm::vec3& half_vector, const glm::vec3& normal, Real roughness)
        {
            // Trowbridge-Reitz GGX; uses roughness^2 -> roughness^4 as in your code
            const auto roughness2 = roughness * roughness;
            const auto roughness4 = roughness2 * roughness2;

            const auto half_cosine = glm::max(glm::dot(normal, half_vector), 0.f);
            const auto half_cosine2 = half_cosine * half_cosine;

            const auto denom = glm::pi<Real>() * glm::pow(half_cosine2 * (roughness4 - 1.f) + 1.f, 2.f);
            return roughness4 / glm::max(denom, EPSILON_F);
        }

        static Real compute_smith_G(const glm::vec3& light, const glm::vec3& view, const glm::vec3& normal, Real roughness)
        {
            //  https://schuttejoe.github.io/post/ggximportancesamplingpart2/

            const auto light_angle = glm::max(glm::dot(normal, light), 0.f);
            const auto view_angle = glm::max(glm::dot(normal, view), 0.f);

            const auto k = (roughness + 1.f) * (roughness + 1.f) / 8.f;

            const auto G1_light = light_angle / (light_angle * (1.f - k) + k);
            const auto G1_view = view_angle / (view_angle * (1.f - k) + k);

            return G1_light * G1_view;
        }

        static glm::vec3 sample_GGX_half_vector(const glm::vec3& normal, Real roughness)
        {
            // GGX NDF sampling in spherical coords, then to world

            const auto u = glm::linearRand(0.f, 1.f);
            const auto v = glm::linearRand(0.f, 1.f);

            const auto phi = 2.f * glm::pi<Real>() * u;

            const auto cosine = glm::sqrt((1.f - v) / (1.f + (roughness * roughness - 1.f) * v));
            const auto sine = glm::sqrt(glm::max(0.f, 1.f - cosine * cosine));

            const auto tangent = build_tangent(normal);
            const auto bitangent = glm::normalize(glm::cross(normal, tangent));

            // Euler angles
            const auto local_coordinates = glm::vec3
            { 
                sine * glm::cos(phi), 
                sine * glm::sin(phi), 
                cosine
            };

            const auto world_coordinates = glm::mat3{ tangent, bitangent, normal } * local_coordinates;
            return glm::normalize(world_coordinates);
        }
    
    private:
        glm::vec3 shade_ggx(const glm::vec3& incoming, const RayIntersection& intersection) const
        {
            static constexpr auto ROUGHNESS_MINIMUM = .1f;
            static constexpr auto VIEW_ANGLE_MINIMUM = .001f;
        
            const auto roughness = glm::max(intersection.material.roughness, ROUGHNESS_MINIMUM);

            const auto view = -incoming;
            const auto normal = intersection.normal;
            const auto half = sample_GGX_half_vector(normal, roughness);
            const auto reflection = glm::reflect(normal, -incoming);

            const auto D = compute_GGX_D(half, normal, roughness);
            const auto G = compute_smith_G(reflection, view, normal, roughness);

            const auto F0 = glm::mix(glm::vec3{ NONMETAL_REFLECTANCE }, intersection.object->material.albedo, intersection.object->material.metallicity);
            const auto F = compute_fresnel_F(F0, glm::max(glm::dot(half, view), 0.f));

            const auto reflection_angle = glm::max(glm::dot(normal, reflection), 0.f);
            const auto view_angle = glm::clamp(glm::dot(normal, view), VIEW_ANGLE_MINIMUM, 1.f);

            const auto ggx = (D * G * F) / (4.f * reflection_angle * view_angle);
            return ggx;
        };

    private:
        ShaderResult evaluate(const glm::vec3& incoming, const RayIntersection& intersection, std::int32_t) const override
        {
            #ifdef ENABLE_GGX_SPECULAR

            const auto specular = shade_ggx(incoming, intersection);

            #else

            const auto normal_angle = glm::clamp(glm::dot(incoming, intersection.normal), 0.f, 1.f);

            const auto F0 = glm::mix(glm::vec3{ NONMETAL_REFLECTANCE }, intersection.object->material.albedo, intersection.object->material.metallicity);
            const auto F = MetallicShader::compute_fresnel_F(F0, 1.f - normal_angle);
            const auto specular = F;

            #endif
            
            const auto reflection_origin = intersection.position + intersection.normal * EPSILON_F;

            const auto reflection = glm::reflect(incoming, intersection.normal);
            const auto reflection_ray = reflection + random_in_unit_sphere(intersection.normal) * intersection.material.roughness;
                
            const auto outgoing = Ray
            {
                .origin = reflection_origin,
                .direction = glm::normalize(reflection_ray),
            };
            
            return ShaderResult
            {
                .absorption = specular,
                .outgoing = outgoing,
                .intersection = intersection,
            };
        }

        Real pdf(const glm::vec3& incoming, const glm::vec3& outgoing, const RayIntersection& intersection) const override
        {
            const auto N = intersection.normal;
            const auto V = -incoming;
            const auto L = outgoing;

            const auto H = glm::normalize(V + L);

            const auto roughness = glm::max(intersection.object->material.roughness, EPSILON_F);

            const auto normal_cosine = glm::max(glm::dot(N, H), 0.f);
            const auto view_cosine = glm::max(glm::dot(V, H), 0.f);

            const auto D = compute_GGX_D(H, N, roughness);

            const auto pdf = (D * normal_cosine) / glm::max(4.f * view_cosine, EPSILON_F);
            return pdf;
        }
    };

    class DielectricShader : public Shader
    {
    private:
        ShaderResult evaluate(const glm::vec3& incoming, const RayIntersection& intersection, std::int32_t) const override
        {
            const auto NdotI = glm::dot(intersection.normal, -incoming);
            const auto is_front_face = NdotI > 0.f;
            const auto normal_angle = glm::clamp(NdotI, 0.f, 1.f);

            const auto F0 = glm::mix(glm::vec3{ NONMETAL_REFLECTANCE }, intersection.object->material.albedo, intersection.object->material.metallicity);
            const auto F = MetallicShader::compute_fresnel_F(F0, 1.f - normal_angle);

            // refraction probability with Schlick's approximation
            const auto ior = intersection.material.refraction_index;
            auto reflectance = (1.f - ior) / (1.f + ior);
            reflectance *= reflectance;

            const auto schlick = reflectance + (1.f - reflectance) * glm::pow(1.f - normal_angle, 5.f);

            const auto is_total_internal_reflection = (ior > 1.f) && (1.f - normal_angle * normal_angle) > (ior * ior);

            auto absorption = glm::vec3{ 1.f };
            auto outgoing = Ray{};

            if (is_total_internal_reflection || glm::linearRand(0.f, 1.f) < schlick)
            {
                // dielectric reflection or total internal reflection

                #ifdef ENABLE_GGX_SPECULAR

                //const auto specular = shade_ggx();
                const auto specular = F;

                #else

                const auto specular = F;

                #endif

                const auto reflection = glm::reflect(incoming, intersection.normal);

                const auto reflection_origin = intersection.position + intersection.normal * EPSILON_F;
                const auto reflection_ray = reflection + random_in_unit_sphere(intersection.normal) * intersection.material.roughness;

                outgoing = Ray
                {
                    .origin = reflection_origin,
                    .direction = glm::normalize(reflection_ray),
                };

                absorption = specular;
            }
            else
            {
                // dielectric refraction

                const auto eta = glm::dot(intersection.normal, -incoming) > 0.f 
                    ? (1.f / intersection.material.refraction_index) 
                    : intersection.material.refraction_index;

                outgoing.origin = intersection.position + (is_front_face ? -intersection.normal : intersection.normal) * EPSILON_F;
                    
                const auto refraction = glm::refract(-incoming, intersection.normal, eta);
                const auto refraction_ray = refraction + random_in_unit_sphere(intersection.normal) * intersection.material.roughness;
                outgoing.direction = glm::normalize(refraction_ray);

                // Beer-Lambert attenuation (re-using albedo as media absorption color)
                const auto attenuation_distance = intersection.exit - intersection.depth;
                const auto attenuation = glm::exp(-intersection.material.albedo * attenuation_distance);

                absorption = attenuation * intersection.material.transmission;
            }

            return ShaderResult
            {
                .absorption = absorption,
                .outgoing = outgoing,
                .intersection = intersection,
            };
        }

        Real pdf(const glm::vec3& incoming, const glm::vec3& outgoing, const RayIntersection& intersection) const override
        {
            const auto normal_angle = glm::clamp(glm::dot(intersection.normal, -incoming), 0.f, 1.f);

            const auto F0 = compute_base_reflectance(intersection.object->material);
            const auto F = MetallicShader::compute_fresnel_F(F0, normal_angle);
            const auto F_mean = glm::clamp((F.r + F.g + F.b) / 3.f, 0.f, 1.f);

            const auto is_reflection = glm::dot(outgoing, intersection.normal) > 0.f;
            return is_reflection ? F_mean : (1.f - F_mean);
        }
    };

    class NextEventEstimationShader : public Shader
    {
    public:
        NextEventEstimationShader(const std::vector<MeshInstance*>& scene_instances)
        {
            for (auto instance : scene_instances)
            {
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

            const auto total = std::accumulate(emissive_objects.begin(), emissive_objects.end(), 0.f, 
                [&](auto sum, auto emitter) { return sum + compute_emissivity(emitter); });

            for (auto& emitter : emissive_objects)
            {
                // pre-compute the probability of sampling each emitter weighted by emissivity as part of importance sampling
                emitter.probability = compute_emissivity(emitter) / total;
            }

            auto prefix_sum = 0.f;
            for (auto& emitter : emissive_objects)
            {
                prefix_sum += emitter.probability;
                cdf.push_back(prefix_sum);
            }

            if (!cdf.empty())
            {
                cdf.back() = 1.f;
            }
        }

    private:
        struct Emitter
        {
            Object* object = nullptr;
            Real power = 0.f;
            Real probability = 0.f;
        };

    private:
        std::vector<Emitter> emissive_objects;
        std::vector<Real> cdf;

    private:
        Real compute_emissivity(const Emitter& emitter)
        {
            return emitter.object->area * glm::length(emitter.object->material.emission);
        };

        int sample_emitter_index() const
        {
            if (cdf.empty()) 
            {
                return -1;
            }

            const auto u = glm::linearRand(0.f, 1.f - std::numeric_limits<Real>::epsilon());
            const auto it = std::upper_bound(cdf.begin(), cdf.end(), u);
            
            int idx = int(it - cdf.begin());
            if (idx >= int(emissive_objects.size()))
            {
                idx = int(emissive_objects.size()) - 1;
            }
            
            return idx;
        }

        ShaderResult evaluate(const glm::vec3& incoming, const RayIntersection& intersection, std::int32_t bounces) const override
        {
            auto path = glm::vec3{ 0.f };

            if (!emissive_objects.empty())
            {
                const int index = sample_emitter_index();
                if (index >= 0)
                {
                    const auto& sampled_emitter = emissive_objects[index];

                    // direct light importance sampling https://raytracing.github.io/books/RayTracingTheRestOfYourLife.html#samplinglightsdirectly/
                    const auto light_sample = sampled_emitter.object->sample();
                    const auto light_normal = sampled_emitter.object->normal_of(light_sample);

                    auto light_direction = light_sample - intersection.position;
                    auto distance2 = glm::length2(light_direction);
                    light_direction = glm::normalize(light_direction);

                    const auto normal_cosine = glm::clamp(glm::dot(intersection.normal, light_direction), 0.f, 1.f);
                    const auto light_cosine = glm::clamp(glm::dot(light_normal, -light_direction), 0.f, 1.f);

                    if (normal_cosine > 0.f && light_cosine > 0.f)
                    {
                        // next-event estimation direct light sampling per bounce
                        // https://www.cg.tuwien.ac.at/sites/default/files/course/4411/attachments/08_next%20event%20estimation.pdf
                        auto light_ray = Ray
                        {
                            .origin = intersection.position + intersection.normal * EPSILON_F,
                            .direction = light_direction,
                        };

                        const auto irradiance = sampled_emitter.object->material.emission;
                        const auto light_area = sampled_emitter.object->area;
                        const auto probability = sampled_emitter.probability;
                        const auto pdf = (probability * distance2) / (light_cosine * light_area);

                        const auto hit = _tlas->intersect(light_ray);
                        if (hit.hit && hit.object == sampled_emitter.object)
                        {
                            // NEE works for diffuse surfaces only, so albedo is the only required information
                            auto albedo = intersection.material.albedo;
                            if (intersection.material.texture)
                            {
                                const auto& uv = intersection.uv;
                                const auto tex = intersection.material.texture->Sample(uv.x, uv.y, intersection.position);
                                albedo = glm::vec3{ tex.r, tex.g, tex.b } / 255.f;
                            }
                            const auto absorption = albedo / glm::pi<Real>();
                            path = (absorption * irradiance * normal_cosine) / pdf;
                        }
                    }
                }
            }

            return ShaderResult
            {
                .absorption = path,
                .outgoing = intersection.outgoing,
                .intersection = intersection,
            };
        }

        Real pdf(const glm::vec3& incoming, const glm::vec3& outgoing, const RayIntersection& intersection) const override
        {
            return 1.f;
        }
    };

    RayIntersection compute_nearest_intersection(const Ray& ray)
    {
        return _tlas->intersect(ray);
    }

    glm::vec3 trace(Ray& ray, RayIntersection& output_intersection, std::int32_t bounces)
    {
        if (bounces <= 0)
        {
            return glm::vec3{ 0.f };
        }

        auto nearest_intersection = compute_nearest_intersection(ray);
        if (bounces == _bounces && nearest_intersection.hit)
        {
            output_intersection = nearest_intersection;
        }

        if (nearest_intersection.hit)
        {
            // NOTE: evidently cannot draw from the parallelized loop: gets malloc_break seg-faults
            //DrawRectDecal({ 1.f, 4.f }, { 2.f, 2.f }, olc::BLUE);

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

            const auto& material = nearest_intersection.material;

            const auto normal_angle = glm::clamp(glm::dot(normal, ray.direction), 0.f, 1.f);

            // using an arbitrary base reflectance of 4% for non-metals, reasonable according to https://docs.omniverse.nvidia.com/materials-and-rendering/latest/templates/parameters/OmniPBR_Reflectivity.html
            const auto F0 = glm::mix(glm::vec3{ NONMETAL_REFLECTANCE }, albedo, material.metallicity);
            const auto F = MetallicShader::compute_fresnel_F(F0, 1.f - normal_angle);

            const auto transmission = material.transmission;

            const auto metallic_probability = material.metallicity;
            const auto dielectric_probability = (1.f - material.metallicity) * transmission;
            const auto diffuse_probability = (1.f - material.metallicity) * (1.f - transmission);

            const auto total = metallic_probability + dielectric_probability + diffuse_probability;

            const auto metallic_weight = metallic_probability / total;
            const auto dielectric_weight = dielectric_probability / total;
            const auto diffuse_weight = diffuse_probability / total;

            const auto random = glm::linearRand(0.f, 1.f);

            auto path = glm::vec3{ 0.f };

            auto sigmoid = [](Real x, Real coefficient)
            {
                const auto saturated = glm::clamp(x, 0.f, 1.f);
                return glm::pow(saturated, coefficient) / (glm::pow(saturated, coefficient) + glm::pow(1.f - saturated, coefficient));
            };

            // NEE only works well for diffuse-type surfaces
            const auto nee_probability = sigmoid(1.f - material.metallicity, 3.f);

            #define ENABLE_NEE
            #ifdef ENABLE_NEE

            if (random > nee_probability)
            {
                // NEE sampling

                static auto next_event_estimation_shader = new NextEventEstimationShader{ _scene_instances };
                const auto nee_sample = next_event_estimation_shader->sample(ray.direction, nearest_intersection, bounces - 1);
                output_intersection.outgoing = nee_sample.outgoing;

                path += nee_sample.absorption;
            }
            else

            #endif
            {
                // BRDF sampling

                auto absorption = glm::vec3{ 1.f };
                auto material_pdf = 1.f;

                auto intersection = nearest_intersection;
                intersection.normal = normal;

                if (random < metallic_weight)
                {
                    // metallic reflection

                    static auto metallic_shader = new MetallicShader{};
                    const auto metallic_sample = metallic_shader->sample(ray.direction, intersection, bounces - 1);
                    output_intersection.outgoing = metallic_sample.outgoing;

                    absorption = metallic_sample.absorption;
                    material_pdf = 1.f / metallic_weight;
                }
                else if (random < metallic_weight + dielectric_weight)
                {
                    // dielectric reflection/refraction

                    static auto dielectric_shader = new DielectricShader{};
                    const auto dielectric_sample = dielectric_shader->sample(ray.direction, intersection, bounces - 1);
                    output_intersection.outgoing = dielectric_sample.outgoing;

                    absorption = dielectric_sample.absorption;
                    material_pdf = 1.f / dielectric_weight;
                }
                else
                {
                    // diffuse scattering

                    static auto diffuse_shader = new DiffuseShader{};
                    const auto diffuse_sample = diffuse_shader->sample(ray.direction, intersection, bounces - 1);
                    output_intersection.outgoing = diffuse_sample.outgoing;

                    absorption = diffuse_sample.absorption;
                    material_pdf = 1.f / diffuse_weight;
                }

                const auto sample = trace(output_intersection.outgoing, output_intersection, bounces - 1);
                const auto brdf = (absorption * sample) / material_pdf;

                path += brdf;
            }
            
            return path;
        }

        // miss, sample skybox texture map
        static auto environment_shader = new EnvironmentShader{};
        const auto skybox = environment_shader->sample(ray.direction, nearest_intersection, bounces - 1);
        return skybox.absorption;
    }
}

#endif
