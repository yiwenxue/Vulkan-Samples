#include "hair_system.h"
#include "util.h"

namespace hair_system
{

bool HairMethod::prepare()
{
	return true;
}

HairSystem::~HairSystem()
{
	if (get_device())
	{
		vkDestroyDescriptorSetLayout(get_device()->get_handle(), _global_ds_layout, nullptr);

		_global_uniform_buffer.reset();
	}
	for (auto &shader_module : _shader_modules)
	{
		vkDestroyShaderModule(get_device()->get_handle(), shader_module, nullptr);
	}
}

void HairSystem::prepare_uniform_buffers()
{
	_global_uniform_buffer = std::make_unique<vkb::core::Buffer>(
	    *get_device(),
	    sizeof(_global_data),
	    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	    VMA_MEMORY_USAGE_CPU_TO_GPU);

	_global_uniform_buffer->convert_and_update(_global_data);

	_time_uniform_buffer = std::make_unique<vkb::core::Buffer>(
	    *get_device(),
	    sizeof(_time_data),
	    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	    VMA_MEMORY_USAGE_CPU_TO_GPU);

	_global_data.light_pos = glm::vec4(0.0);
	_global_data.projection = glm::mat4(1.0);
	_global_data.view = glm::mat4(1.0);

	_time_data.total_time = 0.0f;
	_time_data.delta_time = 0.0f;

	_time_uniform_buffer->convert_and_update(_time_data);
}

void HairSystem::update_uniform_buffers()
{
	_global_uniform_buffer->convert_and_update(_global_data);
	_time_uniform_buffer->convert_and_update(_time_data);
}

void HairSystem::prepare_descriptor_set_layout()
{
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
	};
	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device()->get_handle(), &descriptor_layout, nullptr, &_global_ds_layout));
}

void HairSystem::prepare_descriptor_set()
{
	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        *get_descriptor_pool(),
	        &_global_ds_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(get_device()->get_handle(), &alloc_info, &_global_ds));

	VkDescriptorBufferInfo global_descriptor = create_descriptor(*_global_uniform_buffer.get());
	VkDescriptorBufferInfo time_descriptor   = create_descriptor(*_time_uniform_buffer.get());

	std::vector<VkWriteDescriptorSet> global_descriptors;

	global_descriptors = {
	    vkb::initializers::write_descriptor_set(_global_ds, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &global_descriptor),
	    vkb::initializers::write_descriptor_set(_global_ds, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &time_descriptor),
	};

	vkUpdateDescriptorSets(get_device()->get_handle(), static_cast<uint32_t>(global_descriptors.size()), global_descriptors.data(), 0, NULL);
}

bool HairSystem::prepare()
{
	// _queue_family_index = get_device()->get_queue_family_index(VK_QUEUE_COMPUTE_BIT);

	// Not all implementations support a work group size of 256, so we need to check with the device limits
	// work_group_size = std::min((uint32_t) 256, (uint32_t) get_device()->get_gpu().get_properties().limits.maxComputeWorkGroupSize[0]);
	// Same for shared data size for passing data between shader invocations
	// shared_data_size = std::min((uint32_t) 1024, (uint32_t) (get_device()->get_gpu().get_properties().limits.maxComputeSharedMemorySize / sizeof(glm::vec4)));

	// prepare_compute_descriptor_pool();
	prepare_uniform_buffers();
	prepare_descriptor_set_layout();
	prepare_descriptor_set();

	return true;
}

VkPipelineShaderStageCreateInfo HairSystem::load_shader(const std::string &file, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shader_stage = {};
	shader_stage.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stage.stage                           = stage;
	shader_stage.module                          = vkb::load_shader(file.c_str(), get_device()->get_handle(), stage);
	shader_stage.pName                           = "main";
	assert(shader_stage.module != VK_NULL_HANDLE);
	_shader_modules.push_back(shader_stage.module);
	return shader_stage;
}

}        // namespace hair_system