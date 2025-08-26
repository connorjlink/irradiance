#define OLC_PGE_APPLICATION
#define OLC_IMAGE_STB
#include "olcPixelGameEngine.h"

#include <charconv>
#include <string>

#include "glm/glm.hpp"

#include "utility.h"
#include "renderer.h"

// main.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

using namespace ir;

static constexpr Real MOUSE_SENSITIVITY = 0.1f;
static constexpr Real MOVEMENT_SPEED = 5.f;
static constexpr glm::vec3 UP = glm::vec3{ 0.f, 1.f, 0.f };

class Irradiance : public olc::PixelGameEngine
{
public:
	Irradiance()
	{
		sAppName = "Irradiance";
	}

private:
    std::vector<Object*> scene_objects;

private:
    olc::vi2d last_mouse_position = { 0, 0 };
    bool dirty = false;
    glm::vec3 position = { 0.f, 0.f, 0.f };
    Real fov_degrees = 90.f;
    Real yaw_degrees = 0.f;
    Real pitch_degrees = 0.f;
    int accumulated_frames = 1;
    Ray* rays = nullptr;
    glm::vec3* framebuffer = nullptr;
    bool highlight_dof = false;
    Real dof_distance = std::numeric_limits<Real>::infinity();

private:
    glm::vec3 compute_direction() const
    {
        const auto yaw_radians = glm::radians(yaw_degrees);
        const auto pitch_radians = glm::radians(pitch_degrees);

        const auto x = glm::cos(pitch_radians) * glm::sin(yaw_radians);
        const auto y = glm::sin(pitch_radians);
        const auto z = glm::cos(pitch_radians) * glm::cos(yaw_radians);

        return glm::normalize(glm::vec3{ x, y, z });
    }

    glm::vec3 compute_right() const
    {
        const auto direction = compute_direction();
        return glm::normalize(glm::cross(direction, UP));
    }

public:
	bool OnUserCreate() override
	{
        rays = new Ray[ScreenWidth() * ScreenHeight()];
        framebuffer = new glm::vec3[ScreenWidth() * ScreenHeight()];
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
        if (GetMouse(olc::Mouse::RIGHT).bHeld)
        {
            const auto delta = GetMousePos() - last_mouse_position;

            yaw_degrees += static_cast<Real>(delta.x) * fElapsedTime * MOUSE_SENSITIVITY;

            // negative because y-axis is inverted in screen space
            pitch_degrees -= static_cast<Real>(delta.y) * fElapsedTime * MOUSE_SENSITIVITY;
            // prevents gimbal because the direction uses Euler angles
            pitch_degrees = glm::clamp(pitch_degrees, -80.f, 80.f);

            last_mouse_position = GetMousePos();

            dirty = true;
        }

        auto movement_speed = MOVEMENT_SPEED * (GetKey(olc::Key::SHIFT).bHeld ? 5.f : 1.f);

        if (GetKey(olc::Key::W).bHeld)
        {
            position += compute_direction() * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::S).bHeld)
        {
            position -= compute_direction() * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::A).bHeld)
        {
            const auto right = compute_right();
            position -= right * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::D).bHeld)
        {
            const auto right = compute_right();
            position += right * fElapsedTime * movement_speed;
            dirty = true;
        }

        if (GetKey(olc::Key::TAB).bPressed)
        {
            highlight_dof = !highlight_dof;
        }

        if (dirty)
        {
            // recalculate the camera rays

            const auto aspect_ratio = static_cast<Real>(ScreenWidth()) / static_cast<Real>(ScreenHeight());
            const auto fov_radians = glm::radians(fov_degrees);
            const auto half_height = glm::tan(fov_radians * .5f);
            const auto half_width = aspect_ratio * half_height;

            const auto right = compute_right();

        }

		for (int x = 0; x < ScreenWidth(); x++)
        {
			for (int y = 0; y < ScreenHeight(); y++)
            {
                Draw(x, y, olc::Pixel(rand() % 255, rand() % 255, rand()% 255));	
            }
        }

        last_mouse_position = GetMousePos();
        dirty = false;
        accumulated_frames++;

		return true;
	}

    bool OnUserDestroy() override
    {
        return true;
    }
};

// (c) Connor J. Link. Attribution from personal work outside of ISU.
// Utility function that does not meaningfully affect project functionality.
struct ParseResult
{
	bool success;
	int result;
};
ParseResult parse_int(const std::string& input)
{
	int result = 0;
	const char* begin = input.data();
	const char* end = begin + input.size();
	auto [ptr, ec] = std::from_chars(begin, end, result);
	bool success = (ec == std::errc() && ptr == end);
	return { success, result };
}

int main(int argc, char** argv)
{
    int width = 500, height = 500;

    if (argc > 1)
    {
        for (auto i = 1uz; i < argc; i++) 
        {
            const auto argument = std::string(argv[i]);
            const auto name = argument.substr(0, argument.find('='));
            const auto value = argument.substr(argument.find('=') + 1);

            if (name == "-width")
            {
                const auto result = parse_int(value);
                if (result.success)
                {
                    width = result.result;
                }
            }
            else if (name == "-height")
            {
                const auto result = parse_int(value);
                if (result.success)
                {
                    height = result.result;
                }
            }
        }
    }

	Irradiance application{};
	if (application.Construct(width, height, 1, 1) == olc::OK)
    {
		application.Start();
    }

	return 0;
}
