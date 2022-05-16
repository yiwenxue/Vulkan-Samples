#include "render_system.h"
#include "model.h"
#include "util.h"

namespace hair_system
{

VkPipelineShaderStageCreateInfo Renderer::load_shader(const std::string &file, VkShaderStageFlagBits stage)
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

Renderer::~Renderer()
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

void Renderer::prepare_uniform_buffers()
{
	_global_uniform_buffer = std::make_unique<vkb::core::Buffer>(
	    *get_device(),
	    sizeof(_global_ubo),
	    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	    VMA_MEMORY_USAGE_CPU_TO_GPU);

	_global_uniform_buffer->convert_and_update(_global_ubo);
}

void Renderer::update_uniform_buffers()
{
	_global_uniform_buffer->convert_and_update(_global_ubo);
}

void Renderer::prepare_descriptor_set_layout()
{
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
	};
	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device()->get_handle(), &descriptor_layout, nullptr, &_global_ds_layout));
}

void Renderer::prepare_descriptor_set()
{
	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        *get_descriptor_pool(),
	        &_global_ds_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(get_device()->get_handle(), &alloc_info, &_global_ds));

	printf("global descriptor set: 0x%p\n", _global_ds);

	VkDescriptorBufferInfo buffer_descriptor = create_descriptor(*_global_uniform_buffer.get());

	std::vector<VkWriteDescriptorSet> global_descriptors;

	global_descriptors = {
	    vkb::initializers::write_descriptor_set(_global_ds, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &buffer_descriptor),
	};

	vkUpdateDescriptorSets(get_device()->get_handle(), static_cast<uint32_t>(global_descriptors.size()), global_descriptors.data(), 0, NULL);
}

void Renderer::prepare()
{
	prepare_uniform_buffers();
	prepare_descriptor_set_layout();
	prepare_descriptor_set();
}

void Renderer::render(Scene *scene)
{
}
}        // namespace hair_system