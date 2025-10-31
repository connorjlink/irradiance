#ifndef IRRADIANCE_ACCELERATOR_H
#define IRRADIANCE_ACCELERATOR_H

#include "utility.h"

// accelerator.h
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
}

#endif
