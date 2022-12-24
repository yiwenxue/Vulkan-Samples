#pragma once

#ifdef _WIN32
#	include <corecrt_math_defines.h>
#endif

#include "define.h"

#include "vertex.h"

#include "api_vulkan_sample.h"
#include "vulkan/vulkan_core.h"

namespace hair_system
{

Texture load_texture(vkb::Device *device, const std::string &file);

struct DrawIndirect
{
	uint32_t vertex_count;
	uint32_t instance_count;
	uint32_t first_vertex;
	uint32_t first_instance;
};

struct Particle
{
	glm::vec4 pos;
	glm::vec4 old_pos;
	glm::vec4 velc;

	bool operator==(const Particle &other) const
	{
		return pos == other.pos && old_pos == other.old_pos && velc == other.velc;
	}

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding                         = 0;
		bindingDescription.stride                          = sizeof(Particle);
		bindingDescription.inputRate                       = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

		// Position
		attributeDescriptions[0].binding  = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[0].offset   = offsetof(Particle, pos);

		// Normal
		attributeDescriptions[1].binding  = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[1].offset   = offsetof(Particle, old_pos);

		// Color
		attributeDescriptions[2].binding  = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[2].offset   = offsetof(Particle, velc);

		return attributeDescriptions;
	}
};

struct Model
{
	Model(vkb::Device *device, VkDescriptorPool pool, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);

	void set_texture(Texture texture);

	~Model();

	std::vector<Vertex>                vertices;
	std::unique_ptr<vkb::core::Buffer> vertices_buffer;

	std::vector<uint32_t>              indices;
	std::unique_ptr<vkb::core::Buffer> indices_buffer;

	Texture                            texture;
	std::unique_ptr<vkb::core::Buffer> local_ubo;

	glm::mat4 model_matrix;

	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet       descriptor_set;

	vkb::Device *device{nullptr};
};

struct GridCell
{
	glm::vec4 pos;
	glm::vec4 scale;
};

struct Collider
{
	glm::mat4 transform;
	glm::mat4 inv;
	glm::mat4 invTrans;

	Collider(glm::vec3 trans, glm::vec3 rot, glm::vec3 scale)
	{
		glm::mat4 transMat = glm::translate(trans);
		glm::mat4 rotXMat  = glm::rotate(rot.x * (float) (180.0 / M_PI), glm::vec3(1.0, 0.0, 0.0));
		glm::mat4 rotYMat  = glm::rotate(rot.y * (float) (180.0 / M_PI), glm::vec3(0.0, 1.0, 0.0));
		glm::mat4 rotZMat  = glm::rotate(rot.z * (float) (180.0 / M_PI), glm::vec3(0.0, 0.0, 1.0));
		glm::mat4 scaleMat = glm::scale(scale);

		this->transform = transMat * rotZMat * rotYMat * rotXMat * scaleMat * glm::mat4(1.0);
		this->inv       = glm::inverse(this->transform);
		this->invTrans  = glm::transpose(this->inv);
	}
};

struct Strand
{
	std::vector<Particle> mass_particle;
};

class GridStruct
{
  public:
	GridStruct(vkb::Device *device, VkDescriptorPool pool, glm::vec3 position, glm::vec3 dimensions, glm::ivec3 resolution);

	~GridStruct();

	vkb::Device *device{nullptr};

	std::unique_ptr<Model> grid_model;

	glm::vec3  dimensions;
	glm::ivec3 resolution;

	std::vector<GridCell> cells;

	std::unique_ptr<vkb::core::Buffer> instance_buffer;

	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet       descriptor_set;

	VkDescriptorSetLayout compute_descriptor_set_layout;
	VkDescriptorSet       compute_descriptor_set;

	bool is_instanced{false};

	glm::mat4                          model_matrix;
	std::unique_ptr<vkb::core::Buffer> local_ubo;

	DrawIndirect indirect_data;
};

class ColliderPool
{
  public:
	ColliderPool(vkb::Device *device, VkDescriptorPool pool, std::vector<Collider> &colliders);

	~ColliderPool();

	std::vector<Collider> colliders;

	std::unique_ptr<Model> ellipsoid_model;

	std::unique_ptr<vkb::core::Buffer> instance_buffer;

	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet       descriptor_set;

	bool                               is_instanced{false};
	std::unique_ptr<vkb::core::Buffer> local_ubo;

	vkb::Device *device{nullptr};
};

class Hair
{
  public:
	Hair(vkb::Device *device, VkDescriptorPool pool, std::string file_name);

	~Hair();

	void genStraightHair();

	void genCurlyHair();

	void updateUBO();

	void reset();

	std::vector<Strand> strands;

	std::vector<glm::vec4> new_pos;
	std::vector<glm::vec4> old_pos;
	std::vector<glm::vec4> velc;

	glm::mat4                          model_matrix;
	std::unique_ptr<vkb::core::Buffer> local_ubo;

	HairUBO                            hair_data;
	std::unique_ptr<vkb::core::Buffer> hair_ubo;

	std::unique_ptr<vkb::core::Buffer> pos_buffer;
	std::unique_ptr<vkb::core::Buffer> init_pos_buffer;
	std::unique_ptr<vkb::core::Buffer> old_pos_buffer;
	std::unique_ptr<vkb::core::Buffer> velc_buffer;

	std::unique_ptr<vkb::core::Buffer> indirect_buffer;

	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet       descriptor_set;

	VkDescriptorSetLayout compute_descriptor_set_layout;
	VkDescriptorSet       compute_descriptor_set;

	VkDescriptorSetLayout render_descriptor_set_layout;
	VkDescriptorSet       render_descriptor_set;

	DrawIndirect indirect_data;

	float    hairLength{2.0f};
	uint32_t numStrands{5120};
	uint32_t strandLength{20};

	std::string filename;

	bool is_instanced{false};

	vkb::Device *device{nullptr};
};

}        // namespace hair_system

namespace std
{
template <>
struct hash<hair_system::Particle>
{
	size_t operator()(hair_system::Particle const &particle) const
	{
		return ((hash<glm::vec4>()(particle.pos) ^
		         (hash<glm::vec4>()(particle.old_pos) << 1) ^
		         (hash<glm::vec4>()(particle.velc) << 1)) >>
		        1);
	}
};
}        // namespace std
