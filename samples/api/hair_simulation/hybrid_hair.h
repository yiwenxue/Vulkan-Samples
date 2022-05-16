#pragma once

#include "hair_system.h"

namespace hair_system
{
class HybridHair : public HairMethod
{
  public:
	HybridHair(vkb::Device *device, HairSystem *hair_system);

	virtual ~HybridHair();

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
	VkDescriptorSetLayout descriptor_set_layout;        // Compute shader binding layout
	VkDescriptorSet       descriptor_set;               // Compute shader bindings

	VkPipelineLayout pipeline_layout;            // Layout of the compute pipeline
	VkPipeline       pipeline_integrate;         // Compute pipeline for euler integration (2nd pass)
	VkPipeline       pipeline_constraint;        // Compute pipeline for constraing solve (1st pass)

	bool debug{false};
};
}        // namespace hair_system