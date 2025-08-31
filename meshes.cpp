#include "meshes.h"

// meshes.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    Mesh icosphere(const PBRMaterial& material)
    {
        return load_obj("icosphere.obj", material);
    }

    Mesh torus(const PBRMaterial& material)
    {
        return load_obj("torus.obj", material);
    }

    Mesh cube(const PBRMaterial& material)
    {
        return load_obj("cube.obj", material);
    }

    Mesh cylinder(const PBRMaterial& material)
    {
        return load_obj("cylinder.obj", material);
    }

    Mesh teapot(const PBRMaterial& material)
    {
        return load_obj("teapot.obj", material);
    }

    Mesh monkey(const PBRMaterial& material)
    {
        return load_obj("monkey.obj", material);
    }
}
