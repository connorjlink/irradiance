#ifndef IRRADIANCE_MESHES_H
#define IRRADIANCE_MESHES_H

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "renderer.h"
#include "textures.h"

// meshes.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    std::vector<Object*> icosphere(const glm::mat4& transform = glm::identity<glm::mat4>(), const PBRMaterial& material = PBRMaterial{});
    std::vector<Object*> torus(const glm::mat4& transform = glm::identity<glm::mat4>(), const PBRMaterial& material = PBRMaterial{});
    std::vector<Object*> cube(const glm::mat4& transform = glm::identity<glm::mat4>(), const PBRMaterial& material = PBRMaterial{});
    std::vector<Object*> cylinder(const glm::mat4& transform = glm::identity<glm::mat4>(), const PBRMaterial& material = PBRMaterial{});
    std::vector<Object*> teapot(const glm::mat4& transform = glm::identity<glm::mat4>(), const PBRMaterial& material = PBRMaterial{});
    std::vector<Object*> monkey(const glm::mat4& transform = glm::identity<glm::mat4>(), const PBRMaterial& material = PBRMaterial{});
}

#endif 
