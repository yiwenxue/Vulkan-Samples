#pragma once

#include "define.h"
#include "model.h"
#include "scene.h"

#include "api_vulkan_sample.h"
#include "vulkan/vulkan_core.h"

namespace hair_system
{

class HairSystem;

class HairMethod
{
  public:
	virtual void prepare_compute_descriptor_pool()        = 0;
	virtual void prepare_compute_descriptor_set_layouts() = 0;
	virtual void prepare_compute_descriptor_sets()        = 0;
	virtual void prepare_compute_pipeline()               = 0;

	virtual void prepare_compute_uniform_buffers() = 0;
	virtual void update_compute_uniform_buffers()  = 0;

	virtual bool prepare();

	virtual void simulate(VkCommandBuffer cmd, Scene *scene, float delta_time) = 0;

	virtual bool on_gui_update(vkb::Drawer &drawer) = 0;

	HairSystem *get_hair_system()
	{
		return _hair_system;
	}

  protected:
	HairMethod(vkb::Device *device, HairSystem *hair_system, HairSimuMethod method = HairSimuMethod::UNDEFINED) :
	    _hair_system(hair_system), _method(method)
	{}

  private:
	const HairSimuMethod _method{HairSimuMethod::UNDEFINED};

	HairSystem *_hair_system{nullptr};
};

class HairSystem
{
  public:
	struct LocalUBO
	{
		glm::mat4 model;
	};

	HairSystem(vkb::Device *device, HairSimuMethod _method = HairSimuMethod::UNDEFINED) :
	    device(device)
	{
	}
	~HairSystem();

	void prepare_uniform_buffers();
	void update_uniform_buffers();
	void prepare_descriptor_set_layout();
	void prepare_descriptor_set();

	bool prepare();

	VkPipelineShaderStageCreateInfo load_shader(const std::string &file, VkShaderStageFlagBits stage);

	vkb::Device *get_device()
	{
		return device;
	}

	VkDescriptorSetLayout get_global_ds_layout()
	{
		return _global_ds_layout;
	}

	VkDescriptorSet get_global_ds()
	{
		return _global_ds;
	}

	void set_descriptor_pool(VkDescriptorPool *descriptor_pool)
	{
		_descriptor_pool = descriptor_pool;
	}

	VkDescriptorPool *get_descriptor_pool()
	{
		return _descriptor_pool;
	}

	void set_pipeline_cache(VkPipelineCache *pipeline_cache)
	{
		_pipeline_cache = pipeline_cache;
	}

	VkPipelineCache *get_pipeline_cache()
	{
		return _pipeline_cache;
	}

	GlobalUBO &get_global_ubo()
	{
		return _global_data;
	}

	TimeUBO &get_time_ubo()
	{
		return _time_data;
	}

	uint32_t get_work_group_size()
	{
		return work_group_size;
	}

	uint32_t get_shared_data_size()
	{
		return shared_data_size;
	}

	// data
  private:
	uint32_t work_group_size;
	uint32_t shared_data_size;

	GlobalUBO _global_data;
	TimeUBO   _time_data;

	uint32_t _queue_family_index;

	std::unique_ptr<vkb::core::Buffer> _global_uniform_buffer;
	std::unique_ptr<vkb::core::Buffer> _time_uniform_buffer;

	std::vector<VkShaderModule> _shader_modules;
	VkDescriptorSetLayout       _global_ds_layout;
	VkDescriptorSet             _global_ds;

	vkb::Device      *device{nullptr};
	VkDescriptorPool *_descriptor_pool;
	VkPipelineCache  *_pipeline_cache;
};
}        // namespace hair_system