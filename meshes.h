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
    Mesh icosphere(const PBRMaterial& material = PBRMaterial{});
    Mesh torus(const PBRMaterial& material = PBRMaterial{});
    Mesh cube(const PBRMaterial& material = PBRMaterial{});
    Mesh cylinder(const PBRMaterial& material = PBRMaterial{});
    Mesh teapot(const PBRMaterial& material = PBRMaterial{});
    Mesh monkey(const PBRMaterial& material = PBRMaterial{});
}

#endif 
