#include "model.h"
#include "api_vulkan_sample.h"
#include "glm/detail/type_mat.hpp"
#include "obj_loader.h"
#include "tiny_obj_loader.h"
#include "util.h"

#include "vulkan/vulkan_core.h"
#include <cstdlib>
#include <memory>
#include <vector>

namespace hair_system
{

Texture load_texture(vkb::Device *device, const std::string &file)
{
	Texture texture{};

	texture.image = vkb::sg::Image::load(file, file);
	texture.image->create_vk_image(*device);

	const auto &queue = device->get_queue_by_flags(VK_QUEUE_GRAPHICS_BIT, 0);

	VkCommandBuffer command_buffer = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	vkb::core::Buffer stage_buffer{*device,
	                               texture.image->get_data().size(),
	                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	                               VMA_MEMORY_USAGE_CPU_ONLY};

	stage_buffer.update(texture.image->get_data());

	// Setup buffer copy regions for each mip level
	std::vector<VkBufferImageCopy> bufferCopyRegions;

	auto &mipmaps = texture.image->get_mipmaps();

	for (size_t i = 0; i < mipmaps.size(); i++)
	{
		VkBufferImageCopy buffer_copy_region               = {};
		buffer_copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		buffer_copy_region.imageSubresource.mipLevel       = vkb::to_u32(i);
		buffer_copy_region.imageSubresource.baseArrayLayer = 0;
		buffer_copy_region.imageSubresource.layerCount     = 1;
		buffer_copy_region.imageExtent.width               = texture.image->get_extent().width >> i;
		buffer_copy_region.imageExtent.height              = texture.image->get_extent().height >> i;
		buffer_copy_region.imageExtent.depth               = 1;
		buffer_copy_region.bufferOffset                    = mipmaps[i].offset;

		bufferCopyRegions.push_back(buffer_copy_region);
	}

	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_range.baseMipLevel            = 0;
	subresource_range.levelCount              = vkb::to_u32(mipmaps.size());
	subresource_range.layerCount              = 1;

	// Image barrier for optimal image (target)
	// Optimal image will be used as destination for the copy
	vkb::set_image_layout(
	    command_buffer,
	    texture.image->get_vk_image().get_handle(),
	    VK_IMAGE_LAYOUT_UNDEFINED,
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    subresource_range);

	// Copy mip levels from staging buffer
	vkCmdCopyBufferToImage(
	    command_buffer,
	    stage_buffer.get_handle(),
	    texture.image->get_vk_image().get_handle(),
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    static_cast<uint32_t>(bufferCopyRegions.size()),
	    bufferCopyRegions.data());

	// Change texture image layout to shader read after all mip levels have been copied
	vkb::set_image_layout(
	    command_buffer,
	    texture.image->get_vk_image().get_handle(),
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	    subresource_range);

	device->flush_command_buffer(command_buffer, queue.get_handle());

	// Create a defaultsampler
	VkSamplerCreateInfo sampler_create_info = {};
	sampler_create_info.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_create_info.magFilter           = VK_FILTER_LINEAR;
	sampler_create_info.minFilter           = VK_FILTER_LINEAR;
	sampler_create_info.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_create_info.addressModeU        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeV        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeW        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.mipLodBias          = 0.0f;
	sampler_create_info.compareOp           = VK_COMPARE_OP_NEVER;
	sampler_create_info.minLod              = 0.0f;
	// Max level-of-detail should match mip level count
	sampler_create_info.maxLod = static_cast<float>(mipmaps.size());
	// Only enable anisotropic filtering if enabled on the device
	// Note that for simplicity, we will always be using max. available anisotropy level for the current device
	// This may have an impact on performance, esp. on lower-specced devices
	// In a real-world scenario the level of anisotropy should be a user setting or e.g. lowered for mobile devices by default
	sampler_create_info.maxAnisotropy    = device->get_gpu().get_features().samplerAnisotropy ? (device->get_gpu().get_properties().limits.maxSamplerAnisotropy) : 1.0f;
	sampler_create_info.anisotropyEnable = device->get_gpu().get_features().samplerAnisotropy;
	sampler_create_info.borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK(vkCreateSampler(device->get_handle(), &sampler_create_info, nullptr, &texture.sampler));

	return texture;
}

Model::Model(vkb::Device *device, VkDescriptorPool pool, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices) :
    device(device), vertices(vertices), indices(indices)
{
	vertices_buffer = std::make_unique<vkb::core::Buffer>(
	    *device,
	    vertices.size() * sizeof(Vertex),
	    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_GPU_ONLY);

	indices_buffer = std::make_unique<vkb::core::Buffer>(
	    *device,
	    indices.size() * sizeof(uint32_t),
	    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_GPU_ONLY);

	local_ubo = std::make_unique<vkb::core::Buffer>(
	    *device,
	    sizeof(LocalUBO),
	    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_CPU_TO_GPU);

	auto queue = device->get_queue_by_flags(VK_QUEUE_GRAPHICS_BIT, 0);

	VkDeviceSize buffer_size;
	// staging buffer
	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = vertices.size() * sizeof(Vertex);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(vertices.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), vertices_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}

	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = indices.size() * sizeof(uint32_t);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(indices.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), indices_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}

	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    // Binding 0 : uniform buffer
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        VK_SHADER_STAGE_VERTEX_BIT,
	        0),
	    // Binding 1 : texture
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        VK_SHADER_STAGE_FRAGMENT_BIT,
	        1),
	};

	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(device->get_handle(), &descriptor_layout, nullptr, &descriptor_set_layout));

	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        pool,
	        &descriptor_set_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(device->get_handle(), &alloc_info, &descriptor_set));

	VkDescriptorBufferInfo buffer_descriptor = create_descriptor(*local_ubo.get());

	std::vector<VkWriteDescriptorSet> descriptors{
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &buffer_descriptor),
	};

	vkUpdateDescriptorSets(device->get_handle(), static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, NULL);

	model_matrix = glm::mat4(1.0);
	local_ubo->convert_and_update(model_matrix);
}

Model::~Model()
{
	if (vertices.size() > 0)
	{
		vertices.clear();
	}
	if (indices.size() > 0)
	{
		indices.clear();
	}

	vertices_buffer.reset();
	indices_buffer.reset();
	local_ubo.reset();

	texture.image.reset();

	vkDestroySampler(device->get_handle(), texture.sampler, nullptr);
	vkDestroyDescriptorSetLayout(device->get_handle(), descriptor_set_layout, nullptr);
}

void Model::set_texture(Texture texture)
{
	this->texture = std::move(texture);

	VkDescriptorImageInfo texture_descriptor = create_descriptor(this->texture);

	std::vector<VkWriteDescriptorSet> descriptors{
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texture_descriptor),
	};

	vkUpdateDescriptorSets(device->get_handle(), static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, NULL);
}

uint32_t generate_hair_root(std::string file_name, std::vector<glm::vec3> &pos, std::vector<glm::vec3> &nor, uint32_t samples)
{
	tinyobj::attrib_t                attrib;
	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	std::string file = vkb::fs::path::get(vkb::fs::path::Type::Assets) + file_name;

	// triangulate mesh = true
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file.c_str(), 0, true);

	if (!warn.empty())
	{
		std::cout << warn << std::endl;
	}

	if (!err.empty())
	{
		std::cerr << err << std::endl;
	}

	if (!ret)
	{
		exit(1);
	}

	std::vector<glm::vec3>        positions;
	std::vector<glm::vec3>        normals;
	std::vector<tinyobj::index_t> indices;
	int                           numTriangles = 0;

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++)
	{
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
		{
			int fv = shapes[s].mesh.num_face_vertices[f];

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++)
			{
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				indices.push_back(idx);

				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				positions.push_back(glm::vec3(vx, vy, vz));

				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
				normals.push_back(glm::vec3(nx, ny, nz));
			}
			index_offset += fv;

			// per-face material
			shapes[s].mesh.material_ids[f];

			numTriangles++;
		}
	}

	if (numTriangles < 1)
	{
		return 0;
	}

	srand(8);
	for (int i = 0; i < samples; i++)
	{
		// select triangle at random
		// TODO: account for differences in triangle area?
		int       triangle = rand() % numTriangles;
		int       index    = 3 * triangle;
		glm::vec3 p1       = positions[index];
		glm::vec3 p2       = positions[index + 1];
		glm::vec3 p3       = positions[index + 2];
		glm::vec3 n        = normals[index];        // just use same normal for each vertex of face, won't matter for simulation

		float u = rand() / (float) RAND_MAX;
		float v = rand() / (float) RAND_MAX;

		if (u + v >= 1.f)
		{
			u = 1 - u;
			v = 1 - v;
		}

		glm::vec3 newPos = p1 * u + p2 * v + p3 * (1.f - u - v);

		pos.push_back(newPos);
		nor.push_back(n);
	}

	return samples;
}

Hair::Hair(vkb::Device *device, VkDescriptorPool pool, std::string file_name) :
    device(device)
{
	std::vector<glm::vec3> pos;
	std::vector<glm::vec3> norm;

	numStrands = generate_hair_root(file_name, pos, norm, numStrands);

	Particle particle{};

	float dLength = hairLength / strandLength;

	uint32_t point_num = numStrands * strandLength;

	for (int i = 0; i < numStrands; i++)
	{
		Strand strand;
		// static point
		glm::vec4 dir = glm::vec4(norm[i], 0.0);
		dir[2] -= 2.0f;
		dir[1] += 5.0f;
		dir[0] += 0.05f;

		dir = glm::normalize(dir);

		particle.pos     = glm::vec4(pos[i], 0.0);
		particle.old_pos = glm::vec4(pos[i], 0.0);
		particle.velc    = glm::vec4(0.0);

		strand.mass_particle.resize(strandLength);
		strand.mass_particle[0] = particle;

		new_pos.push_back(particle.pos);
		old_pos.push_back(particle.old_pos);
		velc.push_back(glm::vec4(0.0));

		// // dynamic point
		particle.pos.w     = 1.0;
		particle.old_pos.w = 1.0;

		for (int j = 1; j < strandLength; j++)
		{
			particle.pos            = particle.pos + dir * dLength;
			particle.old_pos        = particle.pos;
			strand.mass_particle[j] = particle;

			new_pos.push_back(particle.pos);
			old_pos.push_back(particle.old_pos);
			velc.push_back(particle.velc);
		}

		strands.push_back(std::move(strand));
	}

	// build strand buffer
	init_pos_buffer = std::make_unique<vkb::core::Buffer>(
	    *device,
	    new_pos.size() * sizeof(glm::vec4),
	    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_GPU_ONLY);

	pos_buffer = std::make_unique<vkb::core::Buffer>(
	    *device,
	    new_pos.size() * sizeof(glm::vec4),
	    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_GPU_ONLY);

	old_pos_buffer = std::make_unique<vkb::core::Buffer>(
	    *device,
	    old_pos.size() * sizeof(glm::vec4),
	    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_GPU_ONLY);

	velc_buffer = std::make_unique<vkb::core::Buffer>(
	    *device,
	    velc.size() * sizeof(glm::vec4),
	    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_GPU_ONLY);

	indirect_buffer = std::make_unique<vkb::core::Buffer>(
	    *device,
	    velc.size() * sizeof(glm::vec4),
	    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_GPU_ONLY);

	local_ubo = std::make_unique<vkb::core::Buffer>(
	    *device,
	    sizeof(LocalUBO),
	    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_CPU_TO_GPU);

	hair_ubo = std::make_unique<vkb::core::Buffer>(
	    *device,
	    sizeof(HairUBO),
	    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_CPU_TO_GPU);

	auto queue = device->get_queue_by_flags(VK_QUEUE_GRAPHICS_BIT, 0);

	VkDeviceSize buffer_size;
	// staging buffer
	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = new_pos.size() * sizeof(glm::vec4);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(new_pos.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), init_pos_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}

	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = new_pos.size() * sizeof(glm::vec4);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(new_pos.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), pos_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}

	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = old_pos.size() * sizeof(glm::vec4);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(old_pos.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), old_pos_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}

	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = velc.size() * sizeof(glm::vec4);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(velc.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), velc_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}

	indirect_data.first_vertex   = 0;
	indirect_data.vertex_count   = numStrands;
	indirect_data.first_instance = 0;
	indirect_data.instance_count = 1;

	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = sizeof(indirect_data);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(&indirect_data, buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), indirect_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}

	// storage
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    // Binding 0 : strand init_pos buffer
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        0),
	    // Binding 1 : strand pos buffer
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        1),
	    // Binding 2 : strand old_pos buffer
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        2),
	    // Binding 3 : strand velc buffer
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        3),
	};

	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(device->get_handle(), &descriptor_layout, nullptr, &descriptor_set_layout));

	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        pool,
	        &descriptor_set_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(device->get_handle(), &alloc_info, &descriptor_set));

	// compute ubo
	set_layout_bindings = {
	    // Binding 0 : local ubo
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        0),
	};
	descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(device->get_handle(), &descriptor_layout, nullptr, &compute_descriptor_set_layout));

	alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        pool,
	        &compute_descriptor_set_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(device->get_handle(), &alloc_info, &compute_descriptor_set));

	// rendering ubo
	set_layout_bindings = {
	    // Binding 0 : local ubo
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        VK_SHADER_STAGE_VERTEX_BIT,
	        0),
	};
	descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(device->get_handle(), &descriptor_layout, nullptr, &render_descriptor_set_layout));

	alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        pool,
	        &render_descriptor_set_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(device->get_handle(), &alloc_info, &render_descriptor_set));

	VkDescriptorBufferInfo init_pos_buffer_descriptor = create_descriptor(*init_pos_buffer.get());
	VkDescriptorBufferInfo pos_buffer_descriptor      = create_descriptor(*pos_buffer.get());
	VkDescriptorBufferInfo old_pos_buffer_descriptor  = create_descriptor(*old_pos_buffer.get());
	VkDescriptorBufferInfo velc_buffer_descriptor     = create_descriptor(*velc_buffer.get());

	VkDescriptorBufferInfo render_uniform_buffer  = create_descriptor(*local_ubo.get());
	VkDescriptorBufferInfo compute_uniform_buffer = create_descriptor(*hair_ubo.get());

	std::vector<VkWriteDescriptorSet> descriptors{
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &init_pos_buffer_descriptor),
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &pos_buffer_descriptor),
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &old_pos_buffer_descriptor),
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &velc_buffer_descriptor),

	    vkb::initializers::write_descriptor_set(render_descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &render_uniform_buffer),

	    vkb::initializers::write_descriptor_set(compute_descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &compute_uniform_buffer),
	};

	vkUpdateDescriptorSets(device->get_handle(), static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, NULL);

	model_matrix = glm::mat4(1.0);

	hair_data.damping       = 0.01;
	hair_data.stiffness     = 0.01;
	hair_data.gravityfactor = -1.0;
	hair_data.timefactor    = 1.0;

	local_ubo->convert_and_update(model_matrix);
	hair_ubo->convert_and_update(hair_data);
}

void Hair::updateUBO()
{
	local_ubo->convert_and_update(model_matrix);
	hair_ubo->convert_and_update(hair_data);
}

void Hair::reset()
{
	auto queue = device->get_queue_by_flags(VK_QUEUE_GRAPHICS_BIT, 0);

	VkDeviceSize buffer_size;
	// staging buffer
	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = new_pos.size() * sizeof(glm::vec4);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(new_pos.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), init_pos_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}

	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = new_pos.size() * sizeof(glm::vec4);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(new_pos.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), pos_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}

	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = old_pos.size() * sizeof(glm::vec4);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(old_pos.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), old_pos_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}

	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = velc.size() * sizeof(glm::vec4);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(velc.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), velc_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}
}

Hair::~Hair()
{
	if (device)
	{
		local_ubo.reset();
		hair_ubo.reset();
		indirect_buffer.reset();

		pos_buffer.reset();
		init_pos_buffer.reset();
		old_pos_buffer.reset();
		velc_buffer.reset();

		vkDestroyDescriptorSetLayout(device->get_handle(), descriptor_set_layout, nullptr);
		vkDestroyDescriptorSetLayout(device->get_handle(), compute_descriptor_set_layout, nullptr);
		vkDestroyDescriptorSetLayout(device->get_handle(), render_descriptor_set_layout, nullptr);
	}

	strands.clear();
}

GridStruct::GridStruct(vkb::Device *device, VkDescriptorPool pool, glm::vec3 position, glm::vec3 dimensions, glm::ivec3 resolution) :
    device(device), dimensions(dimensions), resolution(resolution)
{
	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;

	load_obj("scenes/cube.obj", vertices, indices);

	grid_model = std::make_unique<Model>(device, pool, vertices, indices);
	grid_model->set_texture(load_texture(device, "textures/mannequin_specular.png"));

	uint32_t total_number = resolution.x * resolution.y * resolution.z;

	glm::vec3 step = glm::vec3(1.0) / glm::vec3(resolution);

	glm::vec3 pos = glm::vec3(-0.5, -0.5, -0.5);

	for (int z = 0; z < resolution.z; z++)
	{
		pos.z = -0.5 + z * step.z;
		for (int y = 0; y < resolution.y; y++)
		{
			pos.y = -0.5 + y * step.y;
			for (int x = 0; x < resolution.x; x++)
			{
				pos.x = -0.5 + x * step.x;
				cells.push_back({glm::vec4(pos * dimensions + position, 0.0), glm::vec4(1.0)});
			}
		}
	}

	uint32_t count = total_number / 5;

	srand(8);
	for (int i = 0; i < count; i++)
	{
		uint32_t idx     = rand() % total_number;
		cells[idx].pos.w = 1.0;
	}

	model_matrix = glm::scale(step * dimensions);

	instance_buffer = std::make_unique<vkb::core::Buffer>(
	    *device,
	    total_number * sizeof(GridCell),
	    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_GPU_ONLY);

	local_ubo = std::make_unique<vkb::core::Buffer>(
	    *device,
	    sizeof(LocalUBO),
	    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_CPU_TO_GPU);

	// indirect_buffer = std::make_unique<vkb::core::Buffer>(
	//     *device,
	//     total_number * sizeof(GridCell),
	//     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	//     VMA_MEMORY_USAGE_GPU_ONLY);

	auto queue = device->get_queue_by_flags(VK_QUEUE_GRAPHICS_BIT, 0);

	VkDeviceSize buffer_size;
	// staging buffer
	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = cells.size() * sizeof(GridCell);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(cells.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), instance_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}
	// drawing order

	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    // Binding 0 : model ubo
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        VK_SHADER_STAGE_VERTEX_BIT,
	        0),
	};

	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(device->get_handle(), &descriptor_layout, nullptr, &descriptor_set_layout));

	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        pool,
	        &descriptor_set_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(device->get_handle(), &alloc_info, &descriptor_set));

	set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        0),
	};

	descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(device->get_handle(), &descriptor_layout, nullptr, &compute_descriptor_set_layout));

	alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        pool,
	        &compute_descriptor_set_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(device->get_handle(), &alloc_info, &compute_descriptor_set));

	VkDescriptorBufferInfo render_uniform_buffer  = create_descriptor(*local_ubo.get());
	VkDescriptorBufferInfo compute_storage_buffer = create_descriptor(*instance_buffer.get());

	std::vector<VkWriteDescriptorSet> descriptors{
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &render_uniform_buffer),
	    vkb::initializers::write_descriptor_set(compute_descriptor_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &compute_storage_buffer),
	};

	vkUpdateDescriptorSets(device->get_handle(), static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, NULL);

	local_ubo->convert_and_update(model_matrix);
}

GridStruct::~GridStruct()
{
	if (device)
	{
		local_ubo.reset();

		instance_buffer.reset();

		vkDestroyDescriptorSetLayout(device->get_handle(), descriptor_set_layout, nullptr);
		vkDestroyDescriptorSetLayout(device->get_handle(), compute_descriptor_set_layout, nullptr);
	}

	grid_model.reset();

	cells.clear();
}

ColliderPool::ColliderPool(vkb::Device *device, VkDescriptorPool pool, std::vector<Collider> &colliders) :
    device(device), colliders(std::move(colliders))
{
	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;

	load_obj("scenes/collisionTest.obj", vertices, indices);

	ellipsoid_model = std::make_unique<Model>(device, pool, vertices, indices);
	ellipsoid_model->set_texture(load_texture(device, "textures/mannequin_specular.png"));

	uint32_t count = this->colliders.size();

	instance_buffer = std::make_unique<vkb::core::Buffer>(
	    *device,
	    count * sizeof(Collider),
	    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	    VMA_MEMORY_USAGE_GPU_ONLY);

	auto queue = device->get_queue_by_flags(VK_QUEUE_GRAPHICS_BIT, 0);

	VkDeviceSize buffer_size;
	// staging buffer
	{
		VkCommandBuffer copy_command = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy    copy_region  = {};
		buffer_size                  = count * sizeof(Collider);
		copy_region.size             = buffer_size;
		vkb::core::Buffer staging_buffer{*device, buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY};
		staging_buffer.update(this->colliders.data(), buffer_size);
		vkCmdCopyBuffer(copy_command, staging_buffer.get_handle(), instance_buffer->get_handle(), 1, &copy_region);
		device->flush_command_buffer(copy_command, queue.get_handle(), true);
	}

	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    // Binding 0 : collider shape
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        0),
	};

	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(device->get_handle(), &descriptor_layout, nullptr, &descriptor_set_layout));

	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        pool,
	        &descriptor_set_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(device->get_handle(), &alloc_info, &descriptor_set));

	VkDescriptorBufferInfo collider_buffer = create_descriptor(*instance_buffer.get());

	std::vector<VkWriteDescriptorSet> descriptors{
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &collider_buffer),
	};

	vkUpdateDescriptorSets(device->get_handle(), static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, NULL);
}

ColliderPool::~ColliderPool()
{
	if (device)
	{
		local_ubo.reset();
		instance_buffer.reset();

		vkDestroyDescriptorSetLayout(device->get_handle(), descriptor_set_layout, nullptr);
	}
	ellipsoid_model.reset();
	colliders.clear();
}
}        // namespace hair_system