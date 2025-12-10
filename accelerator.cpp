#include "accelerator.h"
#include "renderer.h"

// accelerator.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace
{
    auto compare_object_along_axis = [](ir::Object* left, ir::Object* right, int axis)
    {
        return left->centroid[axis] < right->centroid[axis];
    };

    auto compare_instance_along_axis = [](ir::MeshInstance* left, ir::MeshInstance* right, int axis)
    {
        return left->volume->centroid[axis] < right->volume->centroid[axis];
    };
}

namespace ir
{
    bool BoundingVolume::contains(const glm::vec3& point) const
    {
        return (point.x >= origin.x) && (point.x <= extent.x) &&
               (point.y >= origin.y) && (point.y <= extent.y) &&
               (point.z >= origin.z) && (point.z <= extent.z);
    }

    bool BoundingVolume::overlaps(const BoundingVolume& other) const
    {
        return (origin.x <= other.extent.x) && (extent.x >= other.origin.x) &&
               (origin.y <= other.extent.y) && (extent.y >= other.origin.y) &&
               (origin.z <= other.extent.z) && (extent.z >= other.origin.z);
    }

    bool BoundingVolume::intersects(const Ray& ray) const
    {
        // Modified slab method https://raytracing.github.io/books/RayTracingTheNextWeek.html#boundingvolumehierarchies/axis-alignedboundingboxes(aabbs)

        const auto direction_safe = ray.direction + glm::vec3{ EPSILON };

        const auto t0 = (origin - ray.origin) / direction_safe;
        const auto t1 = (extent - ray.origin) / direction_safe;

        const auto entry = glm::min(t0, t1);
        const auto exit = glm::max(t0, t1);

        const auto tmin = glm::max(glm::max(entry.x, entry.y), entry.z);
        const auto tmax = glm::min(glm::min(exit.x, exit.y), exit.z);

        return tmax >= tmin;
    }

    BLAS::BLAS(const MeshInstance& meshes)
        : BLAS(meshes.mesh)
    {
    }

    BLAS::BLAS(const std::vector<Object*>& objects)
    {
        if (objects.empty())
        {
            return;
        }

        if (objects.size() == 1)
        {
            // leaf node
            const auto bounds = objects.front()->bounds();
            volume = new BoundingVolume{ bounds->origin, bounds->size, { objects.front() } };
            return;
        }

        // compute a bounding volume for all objects
        auto origin = glm::vec3{ std::numeric_limits<Real>::max() };
        auto extent = glm::vec3{ std::numeric_limits<Real>::lowest() };

        for (const auto& object : objects)
        {
            if (!object)
            {
                continue;
            }

            const auto bounds = object->bounds();
            origin = glm::min(origin, bounds->origin);
            extent = glm::max(extent, bounds->origin + bounds->size);
        }

        volume = new BoundingVolume{ origin, extent - origin };

        // split along the longest axis at the median
        const auto size = extent - origin;
        const auto axis = size.x > size.y ? (size.x > size.z ? 0 : 2) : (size.y > size.z ? 1 : 2);

        std::vector<Object*> left_objects;
        std::vector<Object*> right_objects;

        // IMPORTANT: HAVE TO SORT ALONG THE TARGET AXIS OTHERWISE THE SPLIT IS INVALID!! BAD ARTIFACTS!!!
        // handle unbalanced splits by forcing a balanced split
        std::vector<Object*> sorted_objects = objects;
        std::sort(sorted_objects.begin(), sorted_objects.end(), [&](Object* left, Object* right)
        {
            return left->centroid[axis] < right->centroid[axis];
        });

        const auto half = objects.size() / 2;
        left_objects.insert(left_objects.end(), sorted_objects.begin(), sorted_objects.begin() + half);
        right_objects.insert(right_objects.end(), sorted_objects.begin() + half, sorted_objects.end());

        left = new BLAS{ left_objects };
        right = new BLAS{ right_objects };
    }

    RayIntersection BLAS::intersect(const Ray& ray) const
    {
        if (!volume || !volume->intersects(ray))
        {
            return MISS;
        }

        if (!left && !right)
        {
            // leaf node, hit-test every object in this volume
            
            auto nearest_intersection = MISS;
            auto furthest_intersection = MISS;

            for (const auto& object : volume->contents)
            {
                if (!object)
                {
                    continue;
                }

                if (!object->bound->intersects(ray))
                {
                    continue;
                }

                const auto intersection = object->intersect(ray);

                if (intersection.hit && intersection.depth < nearest_intersection.depth)
                {
                    nearest_intersection = intersection;
                }
                if (intersection.hit && intersection.exit > furthest_intersection.exit)
                {
                    furthest_intersection = intersection;
                }
            }

            if (nearest_intersection.hit && furthest_intersection.hit && nearest_intersection.exit == std::numeric_limits<Real>::infinity())
            {
                nearest_intersection.exit = furthest_intersection.exit;
            }

            return nearest_intersection;
        }

        // branch node, traverse children (~log2 speedup)

        const auto left_intersection = left ? left->intersect(ray) : MISS;
        const auto right_intersection = right ? right->intersect(ray) : MISS;

        if (left_intersection.hit && right_intersection.hit)
        {
            return left_intersection.depth < right_intersection.depth ? left_intersection : right_intersection;
        }
        else if (left_intersection.hit)
        {
            return left_intersection;
        }
        else if (right_intersection.hit)
        {
            return right_intersection;
        }
        
        return MISS;
    }

    TLAS::TLAS(const std::vector<MeshInstance*>& meshes)
    {
        if (meshes.empty())
        {
            return;
        }

        if (meshes.size() == 1)
        {
            // leaf node
            volume = meshes.front()->volume;
            contents.push_back(meshes.front());
            return;
        }

        // compute a bounding volume for all mesh instances
        auto origin = glm::vec3{ std::numeric_limits<Real>::max() };
        auto extent = glm::vec3{ std::numeric_limits<Real>::lowest() };

        for (const auto& instance : meshes)
        {
            if (!instance || !instance->volume)
            {
                continue;
            }

            const auto bounds = instance->volume;
            origin = glm::min(origin, bounds->origin);
            extent = glm::max(extent, bounds->origin + bounds->size);
        }

        volume = new BoundingVolume{ origin, extent - origin };

        // split along the longest axis at the median
        const auto size = extent - origin;
        const auto axis = size.x > size.y ? (size.x > size.z ? 0 : 2) : (size.y > size.z ? 1 : 2);

        std::vector<MeshInstance*> left_instances;
        std::vector<MeshInstance*> right_instances;

        // IMPORTANT: HAVE TO SORT ALONG THE TARGET AXIS OTHERWISE THE SPLIT IS INVALID!! BAD ARTIFACTS!!!
        // handle unbalanced splits by forcing a balanced split
        std::vector<MeshInstance*> sorted_instances = meshes;
        std::sort(sorted_instances.begin(), sorted_instances.end(), [&](MeshInstance* left, MeshInstance* right)
        {
            return left->volume->centroid[axis] < right->volume->centroid[axis];
        });
        const auto half = meshes.size() / 2;
        left_instances.insert(left_instances.end(), sorted_instances.begin(),sorted_instances.begin() + half);
        right_instances.insert(right_instances.end(), sorted_instances.begin() + half,sorted_instances.end());

        left = new TLAS{ left_instances };
        right = new TLAS{ right_instances };
    }

    RayIntersection TLAS::intersect(const Ray& ray) const
    {
        if (!volume || !volume->intersects(ray))
        {
            return MISS;
        }

        if (!left && !right)
        {
            // leaf node, hit-test every mesh instance in this volume

            auto nearest_intersection = MISS;

            for (const auto& instance : contents)
            {
                if (!instance)
                {
                    continue;
                }

                if (!instance->volume->intersects(ray))
                {
                    continue;
                }

                const auto intersection = instance->intersect(ray);

                if (intersection.hit && intersection.depth < nearest_intersection.depth)
                {
                    nearest_intersection = intersection;
                }
            }

            return nearest_intersection;
        }

        // branch node, traverse children (~log2 speedup)

        const auto left_intersection = left ? left->intersect(ray) : MISS;
        const auto right_intersection = right ? right->intersect(ray) : MISS;

        if (left_intersection.hit && right_intersection.hit)
        {
            return left_intersection.depth < right_intersection.depth ? left_intersection : right_intersection;
        }
        else if (left_intersection.hit)
        {
            return left_intersection;
        }
        else if (right_intersection.hit)
        {
            return right_intersection;
        }

        return MISS;
    }
}
