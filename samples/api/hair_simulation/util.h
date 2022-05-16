#pragma once

#include "define.h"

#include "api_vulkan_sample.h"

namespace hair_system
{
VkPipelineShaderStageCreateInfo load_shader(const std::string &file, VkShaderStageFlagBits stage, vkb::Device *device, std::vector<VkShaderModule> &shader_modules);
VkDescriptorImageInfo           create_descriptor(Texture &texture, VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
VkDescriptorBufferInfo          create_descriptor(vkb::core::Buffer &buffer, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
void                            bind_image(vkb::Device *device, Texture *texture, VkDescriptorSet set, uint32_t binding, uint32_t array_element = 0);

}        // namespace hair_system