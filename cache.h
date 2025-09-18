#ifndef IRRADIANCE_CACHE_H
#define IRRADIANCE_CACHE_H

#include <vector>
#include <print>

#include "utility.h"
#include "olcPixelGameEngine.h"

#include "glm/glm.hpp"
#include "glm/vector_relational.hpp"
#include "glm/gtx/norm.hpp"

// cache.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    // hybrid spatial radiance-temporal ray-reconstruction cache
    // represents a voxel grid in world space of 

    struct CacheEntry
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 incidence;
        glm::vec3 radiance;
        Real weight = 0.f;
        Real last_used = -std::numeric_limits<Real>::infinity();
    };

    class RadianceCache
    {
    private:
        glm::vec3 origin;
        glm::vec3 extent;
        glm::vec3 size;
        glm::ivec3 voxel_count;
        Real resolution;
    
    private:
        std::vector<CacheEntry> entries;

    private:
        glm::ivec3 voxel_index(const glm::vec3& position) const
        {
            const auto relative = (position - origin) / resolution;
            return glm::ivec3(glm::floor(relative));
        }

        std::size_t flat_index(const glm::ivec3& voxel) const
        {
            return static_cast<std::size_t>(voxel.x + voxel.y * voxel_count.x + voxel.z * voxel_count.x * voxel_count.y);
        }

    public:
        void insert(const CacheEntry& entry)
        {
            if (glm::any(glm::lessThan(entry.position, origin)) || glm::any(glm::greaterThan(entry.position, extent))) 
            {
                return;
            }

            const auto index = voxel_index(entry.position);
            const auto flat = flat_index(index);

            if (flat >= static_cast<int>(entries.size()))
            {
                return;
            }

            auto& current = entries[flat];

            // temporal blending
            if (current.last_used > -std::numeric_limits<Real>::infinity())
            {
                current.position = (current.position * current.weight + entry.position * entry.weight) / (current.weight + entry.weight);
                current.normal = glm::normalize((current.normal * current.weight + entry.normal * entry.weight) / (current.weight + entry.weight));
                current.incidence = glm::normalize((current.incidence * current.weight + entry.incidence * entry.weight) / (current.weight + entry.weight));
                current.radiance = (current.radiance * current.weight + entry.radiance * entry.weight) / (current.weight + entry.weight);
                current.weight += entry.weight;
                current.last_used = entry.last_used;
            }
            else
            {
                current = entry;
            }
        }

        #pragma message("TODO: incorporate normal and incidence into query for optimization and better matching (quickly reject bad dot product for ray direction/normal alignment)")
        template<std::size_t VoxelThreshold = 0>
        std::optional<CacheEntry> query(const glm::vec3& position) const
        {
            if (glm::any(glm::lessThan(position, origin)) || glm::any(glm::greaterThan(position, extent))) 
            {
                return std::nullopt;
            }

            const auto base_index = voxel_index(position);

            for (auto x = base_index.x - VoxelThreshold; x <= base_index.x + VoxelThreshold; x++)
            {
                if (x < 0 || x >= voxel_count.x)
                    continue;

                for (auto y = base_index.y - VoxelThreshold; y <= base_index.y + VoxelThreshold; y++)
                {
                    if (y < 0 || y >= voxel_count.y)
                        continue;

                    for (auto z = base_index.z - VoxelThreshold; z <= base_index.z + VoxelThreshold; z++)
                    {
                        if (z < 0 || z >= voxel_count.z)
                            continue;

                        const auto index = flat_index(glm::ivec3{ x, y, z });

                        if (index < 0 || index >= static_cast<int>(entries.size()))
                        {
                            continue;
                        }

                        const auto& entry = entries[static_cast<std::size_t>(index)];
                        if (glm::length2(entry.position - position) <= (.25f * resolution * resolution))
                        {
                            return entry;
                        }
                    }
                }
            }

            return std::nullopt;
        }

    public:
        RadianceCache() = default;
        RadianceCache(const glm::vec3& origin, const glm::vec3& extent, Real resolution)
            : origin{ origin }, extent{ extent }, resolution{ resolution }
        {
            #define TESTING
            #ifdef TESTING

            this->origin = glm::vec3{ -20.f };
            this->extent = glm::vec3{ 20.f };

            #endif

            size = (this->extent - this->origin);
            voxel_count = glm::ivec3(glm::max(glm::vec3{1.f}, glm::ceil(size / this->resolution)));

            const auto total =
                static_cast<std::size_t>(voxel_count.x) *
                static_cast<std::size_t>(voxel_count.y) *
                static_cast<std::size_t>(voxel_count.z);

            entries.clear();
            entries.resize(total, {});
        }
    };
}

#endif
