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
        Real weight;
        Real last_used;
    };

    class RadianceCache
    {
    private:
        glm::vec3 origin;
        glm::vec3 extent;
        glm::vec3 size;
        Real resolution;
    
    private:
        std::vector<CacheEntry> entries;

    private:
        glm::ivec3 voxel_index(const glm::vec3& position) const
        {
            return glm::clamp(glm::ivec3{ glm::floor((position - origin) * resolution) }, glm::ivec3{ 0 }, glm::ivec3{ size * resolution } - glm::ivec3{ 1 });
        }

        std::size_t flat_index(const glm::ivec3& voxel) const
        {
            return static_cast<std::size_t>(voxel.x + voxel.y * static_cast<int>(size.x) + voxel.z * static_cast<int>(size.x) * static_cast<int>(size.y));
        }

    public:
        void insert(const CacheEntry& entry)
        {
            #pragma message("TODO: abstract common error handling")
            if (glm::any(glm::lessThan(entry.position, origin)) || glm::any(glm::greaterThan(entry.position, extent))) 
            {
                std::println("Radiance cache query position out of bounds.");
                return;
            }

            const auto index = voxel_index(entry.position);
            const auto flat = flat_index(index);

            auto& current = entries[flat];

            // temporal blending
            if (glm::abs(current.last_used - entry.last_used) < 1.f)
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
                std::println("Radiance cache query position out of bounds.");
                return std::nullopt;
            }

            const auto voxel = glm::clamp(glm::floor((position - origin) * resolution), glm::vec3{ 0.f }, size * resolution - glm::vec3{ 1.f });
            const auto base_index = glm::ivec3{ voxel };

            for (auto x = base_index.x - VoxelThreshold; x <= base_index.x + VoxelThreshold; x++)
            {
                for (auto y = base_index.y - VoxelThreshold; y <= base_index.y + VoxelThreshold; y++)
                {
                    for (auto z = base_index.z - VoxelThreshold; z <= base_index.z + VoxelThreshold; z++)
                    {
                        const auto index = flat_index(glm::ivec3{ x, y, z });

                        if (index < 0 || index >= static_cast<int>(entries.size()))
                        {
                            continue;
                        }

                        const auto& entry = entries[static_cast<std::size_t>(index)];
                        if (glm::length2(entry.position - position) < (1.f / (resolution * resolution)))
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
            size = (extent - origin);
            const auto voxels = size * resolution;
            entries.resize(static_cast<std::size_t>(voxels.x * voxels.y * voxels.z), {});
        }
    };
}

#endif
