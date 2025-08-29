#ifndef IRRADIANCE_TEXTURES_H
#define IRRADIANCE_TEXTURES_H

#include <memory>

#include "olcPixelGameEngine.h"

// textures.h
// (c) 2025 Connor J. Link. All Rights Reserved.

namespace ir
{
    inline std::unique_ptr<olc::Sprite> skybox;
    inline std::unique_ptr<olc::Sprite> water;
    inline std::unique_ptr<olc::Sprite> rock;
    inline std::unique_ptr<olc::Sprite> gemstone;
    inline std::unique_ptr<olc::Sprite> wood;

    inline void initialize_textures()
    {
        skybox = std::make_unique<olc::Sprite>("golden_gate_hills_4k.hdr");
        water = std::make_unique<olc::Sprite>("pexels-enginakyurt-1435752.jpg");
        rock = std::make_unique<olc::Sprite>("pexels-life-of-pix-8892.jpg");
        gemstone = std::make_unique<olc::Sprite>("pexels-jonnylew-1121123.jpg");
        wood = std::make_unique<olc::Sprite>("pexels-fwstudio-33348-129731.jpg");
    }
}

#endif
