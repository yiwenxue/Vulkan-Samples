#include "util.h"

namespace hair_system
{
void bind_image(vkb::Device *device, Texture *texture, VkDescriptorSet set, uint32_t binding, uint32_t array_element)
{
	VkDescriptorImageInfo             image_descriptor = create_descriptor(*texture);
	std::vector<VkWriteDescriptorSet> descriptors;
	descriptors = {vkb::initializers::write_descriptor_set(set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding, &image_descriptor)};
	vkUpdateDescriptorSets(device->get_handle(), static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, NULL);
}

VkDescriptorImageInfo create_descriptor(Texture &texture, VkDescriptorType descriptor_type)
{
	VkDescriptorImageInfo descriptor{};
	descriptor.sampler   = texture.sampler;
	descriptor.imageView = texture.image->get_vk_image_view().get_handle();

	// Add image layout info based on descriptor type
	switch (descriptor_type)
	{
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			if (vkb::is_depth_stencil_format(texture.image->get_vk_image_view().get_format()))
			{
				descriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			else
			{
				descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			break;
		default:
			descriptor.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			break;
	}

	return descriptor;
}

VkDescriptorBufferInfo create_descriptor(vkb::core::Buffer &buffer, VkDeviceSize size, VkDeviceSize offset)
{
	VkDescriptorBufferInfo descriptor{};
	descriptor.buffer = buffer.get_handle();
	descriptor.range  = size;
	descriptor.offset = offset;
	return descriptor;
}

VkPipelineShaderStageCreateInfo load_shader(const std::string &file, VkShaderStageFlagBits stage, vkb::Device *device, std::vector<VkShaderModule> &shader_modules)
{
	VkPipelineShaderStageCreateInfo shader_stage = {};
	shader_stage.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stage.stage                           = stage;
	shader_stage.module                          = vkb::load_shader(file.c_str(), device->get_handle(), stage);
	shader_stage.pName                           = "main";
	assert(shader_stage.module != VK_NULL_HANDLE);
	shader_modules.push_back(shader_stage.module);
	return shader_stage;
}

}        // namespace hair_system