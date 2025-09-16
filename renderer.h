#ifndef IRRADIANCE_RENDERER_H
#define IRRADIANCE_RENDERER_H

#include "utility.h"
#include "olcPixelGameEngine.h"

#include "glm/ext/scalar_common.hpp"

// renderer.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    struct BoundingVolume
    {
    public:
        glm::vec3 origin;
        glm::vec3 size;
        glm::vec3 extent;
        glm::vec3 centroid;
        std::vector<Object*> contents;

    public:
        BoundingVolume(const glm::vec3& origin, const glm::vec3& size, const std::vector<Object*>& contents = {})
            : origin{ origin }, size{ size }, contents{ contents }
        {
            centroid = origin + (size * .5f);
            extent = origin + size;
        }

    public:
        bool contains(const glm::vec3& point) const;
        bool overlaps(const BoundingVolume& other) const;
        bool intersects(const Ray& ray) const;
    };

    struct BoundingVolumeHierarchy
    {
    public:
        BoundingVolume* volume = nullptr;
        // left = right = nullptr -> leaf
        BoundingVolumeHierarchy* left = nullptr;
        BoundingVolumeHierarchy* right = nullptr;

    public:
        BoundingVolumeHierarchy() = default;
        BoundingVolumeHierarchy(BoundingVolume* volume, BoundingVolumeHierarchy* left, BoundingVolumeHierarchy* right)
            : volume{ volume }, left{ left }, right{ right }
        {
        }

    public:
        virtual ~BoundingVolumeHierarchy()
        {
            delete volume;
            delete left;
            delete right;
        }

    public:
        virtual RayIntersection intersect(const Ray& ray) const = 0;
    };

    struct MeshInstance;

    // constructed in object space
    struct BLAS : public BoundingVolumeHierarchy
    {
    public:
        BLAS(BoundingVolume* volume, BLAS* left, BLAS* right)
            : BoundingVolumeHierarchy{ volume, left, right }
        {
        }

    public:
        BLAS(const MeshInstance& meshes);
        BLAS(const std::vector<Object*>& objects);

    public:
        RayIntersection intersect(const Ray& ray) const override;
    };

    // constructed in world space
    struct TLAS : public BoundingVolumeHierarchy
    {
    public:
        std::vector<MeshInstance*> contents;

    public:
        TLAS(BoundingVolume* volume, TLAS* left, TLAS* right)
            : BoundingVolumeHierarchy{ volume, left, right }
        {
        }

    public:
        TLAS(const std::vector<MeshInstance*>& meshes);

    public:
        RayIntersection intersect(const Ray& ray) const override;
    };

    struct Object
    {
    public:
        PBRMaterial material;
        Real area = 0.f;
        glm::vec3 centroid = glm::vec3{ 0.f };
        BoundingVolume* bound = nullptr;

    public:
        BoundingVolume* bounds();

    public:
        Object(const PBRMaterial& material) 
            : material{ material }
        {
        }
        virtual ~Object() = default;

    public:
        virtual RayIntersection intersect(const Ray& ray) = 0;
        virtual glm::vec3 sample() = 0;
        virtual glm::vec3 normal_of(const glm::vec3& position) = 0;
    };

    struct Sphere : public Object
    {
    public:
        glm::vec3 center;
        Real radius;

    public:
        Sphere(const glm::vec3& center, Real radius, const PBRMaterial& material)
            : center{ center }, radius{ radius }, Object{ material }
        {
            area = 4.f * glm::pi<Real>() * radius * radius;
            centroid = center;
            bound = new BoundingVolume{ center - glm::vec3{ radius }, glm::vec3{ 2.f * radius }, { this } };
        }

    public:
        RayIntersection intersect(const Ray& ray) override;
        glm::vec3 sample() override;
        glm::vec3 normal_of(const glm::vec3& position) override;
    };

    struct Triangle : public Object
    {
    public:   
        glm::vec3 v0, v1, v2;
        glm::vec3 n0, n1, n2;
        glm::vec2 uv0, uv1, uv2;
        glm::vec3 edge0, edge1;
        Real d00, d01, d11;
        Real denominator;
        glm::vec3 normal;
        bool smoothed = false;

    public:
        Triangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& n0, const glm::vec3& n1, const glm::vec3& n2, const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,  const PBRMaterial& material)
            : Triangle{ v0, v1, v2, uv0, uv1, uv2, material }
        {
            this->n0 = n0;
            this->n1 = n1;
            this->n2 = n2;
            smoothed = true;
        }

        Triangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2, const PBRMaterial& material)
            : v0{ v0 }, v1{ v1 }, v2{ v2 }, uv0{ uv0 }, uv1{ uv1 }, uv2{ uv2 }, Object{ material }
        {
            // cache to avoid recomputing per intersection
            edge0 = v1 - v0;
            edge1 = v2 - v0;
            d00 = glm::dot(edge0, edge0);
            d01 = glm::dot(edge0, edge1);
            d11 = glm::dot(edge1, edge1);
            denominator = (d00 * d11) - (d01 * d01);
            normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
            // area is half the equivalent parallelogram
            area = .5f * glm::length(glm::cross(edge0, edge1));
            centroid = (v0 + v1 + v2) / 3.f;

            const auto origin = glm::min(v0, v1, v2);
            const auto scissor = glm::max(v0, v1, v2) - origin;
            bound = new BoundingVolume{ origin, scissor, { this } };
        }

    public:
        RayIntersection intersect(const Ray& ray) override;
        glm::vec3 sample() override;
        glm::vec3 normal_of(const glm::vec3& position) override;
    };

    struct Quadrilateral : public Object
    {
    public:
        // v0 is a shared origin like Q from Ray Tracing in One Weekend, v1 and v2 are edge vectors therefrom to form a parallelogram
        glm::vec3 v0, v1, v2;
        glm::vec3 normal;
        // Ax + By + Cz = D (constant)
        Real constant;
        glm::vec3 reciprocal;

    public:
        Quadrilateral(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const PBRMaterial& material)
            : v0{ v0 }, Object{ material }
        {
            this->v1 = v0 - v1;
            this->v2 = v0 - v2;

            const auto orthogonal = glm::cross(this->v1, this->v2);
            normal = glm::normalize(orthogonal);
            constant = glm::dot(normal, v0);
            reciprocal = orthogonal / glm::dot(orthogonal, orthogonal);
            // area is equal to cross product parallogram
            area = glm::length(orthogonal);
            // parallelogram centroid: r0 + (u + v) / 2
            centroid = v0 + (this->v1 + this->v2) / 2.f;

            const auto origin = glm::min(v0, v1, v2);
            const auto scissor = glm::max(v0, v1, v2) - origin;
            bound = new BoundingVolume{ origin, scissor, { this } };
        }
    
    public:
        RayIntersection intersect(const Ray& ray) override;
        glm::vec3 sample() override;
        glm::vec3 normal_of(const glm::vec3& position) override;
    }; 

    struct Cuboid : public Object
    {
    public:
        glm::vec3 origin;
        glm::vec3 size;

    public:
        Cuboid(const glm::vec3& origin, const glm::vec3& size, const PBRMaterial& material)
            : origin{ origin }, size{ size }, Object{ material }
        {
            area = 2.f * (size.x * size.y + size.y * size.z + size.z * size.x);
            centroid = origin + size / 2.f;
            bound = new BoundingVolume{ origin, size, { this } };
        }

    public:
        RayIntersection intersect(const Ray& ray) override;
        glm::vec3 sample() override;
        glm::vec3 normal_of(const glm::vec3& position) override;
    };

    struct Quadric : public Object
    {
    public:
        // Ax^2 + By^2 + Cz^2 + Dxy + Exz + Fyz + Gx + Hy + Iz + J = 0
        Real A, B, C, D, E, F, G, H, I, J;
        Cuboid* container;

    public:
        Quadric(Real A, Real B, Real C, Real D, Real E, Real F, Real G, Real H, Real I, Real J, const glm::vec3& origin, const glm::vec3& size, const PBRMaterial& material)
            : A{ A }, B{ B }, C{ C }, D{ D }, E{ E }, F{ F }, G{ G }, H{ H }, I{ I }, J{ J }, Object{ material }
        {
            // quick and dirty approximations since there don't seem to be any easy closed-form solutions
            area = size.x * size.z;
            centroid = origin + size / 2.f;
            container = new Cuboid{ origin, size, material };
            const auto container_bounds = container->bounds();
            bound = new BoundingVolume{ container_bounds->origin, container_bounds->size, { this } };
        }

    private:
        Real function(const glm::vec3& position)
        {
            return (A * position.x * position.x) + (B * position.y * position.y) + (C * position.z * position.z) + 
                   (D * position.x * position.y) + (E * position.x * position.z) + (F * position.y * position.z) +
                   (G * position.x) + (H * position.y) + (I * position.z) + J;
        }

    public:
        RayIntersection intersect(const Ray& ray) override;
        glm::vec3 sample() override;
        glm::vec3 normal_of(const glm::vec3& position) override;
    };

    struct Colloid : public Object
    {
    public:
        Real density;
        Object* container;
        
    public:
        Colloid(Real density, Object* container)
            : density{ density }, container{ container }, Object{ container->material }
        {
            area = container->area;
            centroid = container->centroid;
            bound = container->bound;
        }

    public:
        RayIntersection intersect(const Ray& ray) override;
        glm::vec3 sample() override;
        glm::vec3 normal_of(const glm::vec3& position) override;
    };

    using Mesh = std::vector<Object*>;

    struct MeshInstance
    {
    public:
        glm::mat4 transform;
        glm::mat4 inverse;
        const Mesh& mesh;
        BoundingVolume* volume;

        // bottom level acceleration structure for this mesh instance only built in object space
        BLAS* blas;

    public:
        MeshInstance(const glm::mat4& transform, const Mesh& mesh)
            : transform{ transform }, mesh{ mesh }
        {
            inverse = glm::inverse(transform);

            auto world_space_minimum = glm::vec3{ std::numeric_limits<Real>::max() };
            auto world_space_maximum = glm::vec3{ std::numeric_limits<Real>::lowest() };

            for (const auto& object : mesh)
            {
                if (!object)
                {
                    continue;
                }

                const auto bounds = object->bounds();
                // progressively find the tightest volume containing every sub-object
                world_space_minimum = glm::min(world_space_minimum, glm::vec3{ transform * glm::vec4{ bounds->origin, 1.f } });
                world_space_maximum = glm::max(world_space_maximum, glm::vec3{ transform * glm::vec4{ bounds->origin + bounds->size, 1.f } });
            }

            volume = new BoundingVolume{ world_space_minimum, world_space_maximum - world_space_minimum, mesh };

            blas = new BLAS{ mesh };
        }

    public:
        RayIntersection intersect(const Ray& ray) const;
    };

    Mesh load_obj(const std::string& filepath, const PBRMaterial& default_material);
}

#endif
