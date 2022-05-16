#pragma once

#include "hair_system.h"

namespace hair_system
{
class StrandHair : public HairMethod
{
  public:
	struct LocalUBO
	{
		glm::mat4 model;
		float     damping{0.01};
		float     stiff{0.01};
	};

	StrandHair(vkb::Device *device, HairSystem *hair_system);

	virtual ~StrandHair();

	void prepare_compute_descriptor_pool() override;
	void prepare_compute_descriptor_set_layouts() override;
	void prepare_compute_descriptor_sets() override;
	void prepare_compute_pipeline() override;

	void prepare_compute_uniform_buffers() override;

	void update_compute_uniform_buffers() override;

	bool prepare() override;
	void simulate(VkCommandBuffer cmd, Scene *scene, float delta_time) override;

	bool on_gui_update(vkb::Drawer &drawer) override;

  protected:
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet       descriptor_set;            // model matrix
	VkDescriptorSetLayout user_ds_layout;            // hair data
	VkDescriptorSetLayout compute_ds_layout;         // hair constant
	VkDescriptorSetLayout grid_ds_layout;            // grid data
	VkDescriptorSetLayout collider_ds_layout;        // collider data

	VkPipelineLayout pipeline_layout;
	VkPipeline       pipeline_integrate;
	VkPipeline       pipeline_constraint;

	LocalUBO                           _local_ubo;
	std::unique_ptr<vkb::core::Buffer> _local_buffer;

	bool debug{false};
};
}        // namespace hair_system