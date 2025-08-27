#define OLC_PGE_APPLICATION
#define OLC_IMAGE_STB
#include "olcPixelGameEngine.h"

#include <charconv>
#include <string>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/random.hpp"

#include "utility.h"
#include "renderer.h"

// main.cpp
// (c) 2025 Connor J. Link. All Rights Reserved.

using namespace ir;

static constexpr Real MOUSE_SENSITIVITY = 5.f;
static constexpr Real MOVEMENT_SPEED = 5.f;
static constexpr glm::vec3 UP = glm::vec3{ 0.f, 1.f, 0.f };
static constexpr Real SAMPLE_JITTER = .001f;

int _bounces = 2;
int _samples = 5;

class Irradiance : public olc::PixelGameEngine
{
public:
	Irradiance()
	{
		sAppName = "Irradiance";
	}

private:
    std::vector<Object*> scene_objects = 
    {
        new Sphere
        {
            glm::vec3{ 0.f, 0.f, 5.f },
            1.f,
            PBRMaterial
            {
                .albedo = glm::vec3{ 1.f, 0.f, 0.f },
                .absorption = glm::vec3{ 0.f, 0.f, 0.f },
                .emission = glm::vec3{ 0.f, 0.f, 0.f },
                .metallicity = 0.f,
                .anisotropy = 0.f,
                .roughness = 0.5f,
            }
        },
    };

private:
    olc::vi2d last_mouse_position = { 0, 0 };
    bool dirty = true;
    glm::vec3 position = { 0.f, 0.f, 0.f };
    Real fov_degrees = 90.f;
    Real yaw_degrees = 0.f;
    Real pitch_degrees = 0.f;
    int accumulated_frames = 1;
    Ray* rays = nullptr;
    glm::vec3* framebuffer = nullptr;
    bool highlight_dof = false;
    Real dof_distance = 0.f;

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
        DrawStringPropDecal({ 5.f, 5.f }, std::format("Frames: {} ms/frame: {:.2f}", accumulated_frames, fElapsedTime * 1000.f), olc::YELLOW);
        DrawStringPropDecal({ 5.f, 15.f }, std::format("Position: ({:.2f}, {:.2f}, {:.2f})", position.x, position.y, position.z), olc::YELLOW);
        DrawStringPropDecal({ 5.f, 25.f }, std::format("Yaw: {:.2f} Pitch: {:.2f}", yaw_degrees, pitch_degrees), olc::YELLOW);
        DrawStringPropDecal({ 5.f, 35.f }, std::format("DOF: {} @ {:.2f}", highlight_dof ? "ON" : "OFF", dof_distance), olc::YELLOW);

        const auto& ray = rays[GetMouseX() + GetMouseY() * ScreenWidth()].direction;

        DrawStringPropDecal({ 5.f, 45.f }, std::format("Ray: ({:2f}, {:2f}, {:2f})", ray.x, ray.y, ray.z), olc::YELLOW);

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

        auto movement_speed = MOVEMENT_SPEED * (GetKey(olc::Key::CTRL).bHeld ? 5.f : 1.f);

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
        if (GetKey(olc::Key::SPACE).bHeld)
        {
            position += UP * fElapsedTime * movement_speed;
            dirty = true;
        }
        if (GetKey(olc::Key::SHIFT).bHeld)
        {
            position -= UP * fElapsedTime * movement_speed;
            dirty = true;
        }

        if (GetKey(olc::Key::TAB).bPressed)
        {
            highlight_dof = !highlight_dof;
        }
        if (GetKey(olc::Key::UP).bHeld)
        {
            dof_distance += fElapsedTime * movement_speed * 2.f;
            dirty = true;
        }
        if (GetKey(olc::Key::DOWN).bHeld)
        {
            dof_distance -= fElapsedTime * movement_speed * 2.f;
            dof_distance = glm::max(dof_distance, 0.f);
            dirty = true;
        }

        if (dirty)
        {
            // recalculate the camera rays

            const auto aspect_ratio = static_cast<Real>(ScreenWidth()) / static_cast<Real>(ScreenHeight());
            const auto fov_radians = glm::radians(fov_degrees);

            const auto right = compute_right();

            const auto projection = glm::perspective(fov_radians, aspect_ratio, .1f, 1000.f);
            const auto inverse_projection = glm::inverse(projection);
            const auto view = glm::lookAt(position, position + compute_direction(), UP);
            const auto inverse_view = glm::inverse(view);

        #pragma omp parallel for collapse(2)
            for (int x = 0; x < ScreenWidth(); x++)
            {
                for (int y = 0; y < ScreenHeight(); y++)
                {
                    const auto u = static_cast<Real>(x) / static_cast<Real>(ScreenWidth());
                    const auto v = static_cast<Real>(y) / static_cast<Real>(ScreenHeight());

                    // normalized device coordinates
                    const auto ndc_x = 2.f * u - 1.f;
                    const auto ndc_y = 2.f * v - 1.f;

                    const auto clip_space_pos = glm::vec4{ ndc_x, ndc_y, 1.f, 1.f };
                    const auto world_space_pos_homogeneous = inverse_projection * clip_space_pos;
                    const auto world_space_pos = world_space_pos_homogeneous / world_space_pos_homogeneous.w;
                    const auto world_space_pos_normalized = inverse_view * glm::normalize(glm::vec4{ glm::vec3{ world_space_pos }, 0.f });

                    rays[y * ScreenWidth() + x] = Ray{ position, world_space_pos_normalized };
                }
            }
        }

    #pragma omp parallel for collapse(2)
		for (int x = 0; x < ScreenWidth(); x++)
        {
			for (int y = 0; y < ScreenHeight(); y++)
            {
                const auto& ray = rays[y * ScreenWidth() + x];

                glm::vec3 total_color = glm::vec3{ 0.f, 0.f, 0.f };

                for (int s = 0; s < _samples; s++)
                {
                    // using linear to avoid biasing sampling toward the center of each pixel
                    auto ray_jittered = ray;
                    ray_jittered.direction += glm::linearRand(glm::vec3{ -SAMPLE_JITTER, -SAMPLE_JITTER, -SAMPLE_JITTER }, glm::vec3{ SAMPLE_JITTER, SAMPLE_JITTER, SAMPLE_JITTER });

                    glm::vec3 composite_color = glm::vec3{ 1.f, 1.f, 1.f };

                    for (int b = 0; b < _bounces; b++)
                    {
                        // TODO: bounding volume hierarchy acceleration structure

                        for (const auto& object : scene_objects)
                        {
                            const auto intersection = object->intersect(ray_jittered);

                            if (intersection.hit)
                            {
                                DrawRectDecal({ 1.f, 4.f }, { 2.f, 2.f }, olc::BLUE);

                                // for testing only
                                //std::cout << "Hit at depth: " << intersection.depth << "\n";

                                // TODO: implement full PBR shading model

                                // simple diffuse shading for now
                                composite_color *= intersection.material.albedo;

                                // prepare for next bounce
                                ray_jittered.origin = intersection.position + intersection.normal * .001f; // offset to prevent self-intersection
                                
                                // TODO: incorporate importance and BRDF sampling
                                ray_jittered.direction = glm::normalize(glm::reflect(ray_jittered.direction, intersection.normal));
                            }
                            else
                            {
                                // TODO: environment cube map sampling
                                // accent color for sky
                                //composite_color = glm::vec3{ 143.f / 255.f, 132.f / 255.f, 213.f / 255.f };
                                composite_color = glm::vec3{ 0.f, 0.f, 0.f };
                                break;
                            }
                        }
                    }

                    total_color += composite_color;
                }

                total_color /= static_cast<Real>(_samples);

                Draw(x, y, olc::Pixel(total_color.r * 255, total_color.g * 255, total_color.b * 255));	
            }
        }

        if (dirty)
        {
            DrawRectDecal({ 1.f, 1.f }, { 2.f, 2.f }, olc::GREEN);
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
            else if (name == "-bounces")
            {
                const auto result = parse_int(value);
                if (result.success)
                {
                    _bounces = result.result;
                }
            }
            else if (name == "-samples")
            {
                const auto result = parse_int(value);
                if (result.success)
                {
                    _samples = result.result;
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
