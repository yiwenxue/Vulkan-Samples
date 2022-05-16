#pragma once
#include "core/buffer.h"

#include "vulkan/vulkan_core.h"

#include <chrono>
#include <forward_list>
#include <memory>
#include <vector>

namespace hair_system
{
enum class RenderPipelineType
{
	UNDEFINED,
	MESH,
	WIREFRAME,
	STRAND,
	FLUID,
};

enum class HairSimuMethod
{
	UNDEFINED,
	STRAND,
	HYBRID,
};

struct GlobalUBO
{
	glm::mat4 view;
	glm::mat4 projection;
	glm::vec4 light_pos;
};

struct LocalUBO
{
	glm::mat4 model;
};

struct TimeUBO
{
	float delta_time;
	float total_time;
};

struct HairUBO
{
	float stiffness;
	float damping;
	float gravityfactor;
	float timefactor;
};

}        // namespace hair_system