#include "strand_hair.h"

#include "util.h"
#include "vulkan/vulkan_core.h"

namespace hair_system
{
StrandHair::StrandHair(vkb::Device *device, HairSystem *hair_system) :
    HairMethod(device, hair_system, HairSimuMethod::HYBRID)
{
	prepare();
}

StrandHair::~StrandHair()
{
	vkDestroyPipeline(get_hair_system()->get_device()->get_handle(), pipeline_integrate, nullptr);
	vkDestroyPipeline(get_hair_system()->get_device()->get_handle(), pipeline_constraint, nullptr);

	vkDestroyDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), descriptor_set_layout, nullptr);
	vkDestroyDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), user_ds_layout, nullptr);
	vkDestroyDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), compute_ds_layout, nullptr);
	vkDestroyDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), grid_ds_layout, nullptr);
	vkDestroyDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), collider_ds_layout, nullptr);
	vkDestroyPipelineLayout(get_hair_system()->get_device()->get_handle(), pipeline_layout, nullptr);
}

void StrandHair::prepare_compute_descriptor_pool()
{
}

void StrandHair::prepare_compute_descriptor_set_layouts()
{
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        0),
	};

	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), &descriptor_layout, nullptr, &descriptor_set_layout));

	set_layout_bindings = {
	    // Binding 0 : strand init pos buffer
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

	descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), &descriptor_layout, nullptr, &user_ds_layout));

	set_layout_bindings = {
	    // Binding 0 : collider data
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        0),
	};

	descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), &descriptor_layout, nullptr, &collider_ds_layout));

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

	VK_CHECK(vkCreateDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), &descriptor_layout, nullptr, &compute_ds_layout));

	// compute ubo
	set_layout_bindings = {
	    // Binding 0 : local ubo
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        0),
	};
	descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), &descriptor_layout, nullptr, &grid_ds_layout));

	std::vector<VkDescriptorSetLayout> ds_layouts = {
	    get_hair_system()->get_global_ds_layout(),
	    descriptor_set_layout,
	    user_ds_layout,
	    grid_ds_layout,
	    compute_ds_layout,
	    collider_ds_layout,
	};

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = vkb::initializers::pipeline_layout_create_info(
	    ds_layouts.data(),
	    ds_layouts.size());

	VK_CHECK(vkCreatePipelineLayout(get_hair_system()->get_device()->get_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout));
}

void StrandHair::prepare_compute_descriptor_sets()
{
	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        *get_hair_system()->get_descriptor_pool(),
	        &descriptor_set_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(get_hair_system()->get_device()->get_handle(), &alloc_info, &descriptor_set));

	VkDescriptorBufferInfo buffer_descriptor = create_descriptor(*_local_buffer.get());

	std::vector<VkWriteDescriptorSet> descriptors;

	descriptors = {vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &buffer_descriptor)};

	vkUpdateDescriptorSets(get_hair_system()->get_device()->get_handle(), static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, NULL);
}

void StrandHair::prepare_compute_pipeline()
{
	// integrate

	auto pipeline_cache = *get_hair_system()->get_pipeline_cache();

	auto *device = get_hair_system()->get_device();

	VkComputePipelineCreateInfo compute_pipeline_create_info = vkb::initializers::compute_pipeline_create_info(pipeline_layout, 0);

	compute_pipeline_create_info.stage = get_hair_system()->load_shader("hair_simulation/integrate.comp", VK_SHADER_STAGE_COMPUTE_BIT);

	VK_CHECK(vkCreateComputePipelines(device->get_handle(), pipeline_cache, 1, &compute_pipeline_create_info, nullptr, &pipeline_integrate));

	// constraint
	compute_pipeline_create_info.stage = get_hair_system()->load_shader("hair_simulation/integrate.comp", VK_SHADER_STAGE_COMPUTE_BIT);

	VK_CHECK(vkCreateComputePipelines(device->get_handle(), pipeline_cache, 1, &compute_pipeline_create_info, nullptr, &pipeline_constraint));
}

void StrandHair::prepare_compute_uniform_buffers()
{
	_local_buffer = std::make_unique<vkb::core::Buffer>(
	    *get_hair_system()->get_device(),
	    sizeof(_local_ubo),
	    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	    VMA_MEMORY_USAGE_CPU_TO_GPU);

	_local_buffer->convert_and_update(_local_ubo);
}

void StrandHair::update_compute_uniform_buffers()
{
	_local_buffer->convert_and_update(_local_ubo);
}

bool StrandHair::prepare()
{
	if (!HairMethod::prepare())
	{
		return false;
	}

	prepare_compute_uniform_buffers();

	prepare_compute_descriptor_pool();

	prepare_compute_descriptor_set_layouts();

	prepare_compute_descriptor_sets();

	prepare_compute_pipeline();

	return true;
};

void StrandHair::simulate(VkCommandBuffer cmd, Scene *scene, float delta_time)
{
	uint32_t group_size = 128;

	if (scene->get_hairs().size() == 0 || scene->get_colliders().size() == 0 || scene->get_grids().size() == 0)
	{
		return;
	}

	auto *hair      = scene->get_hairs()[0];
	auto *colliders = scene->get_colliders()[0];
	auto *grids     = scene->get_grids()[0];

	std::vector<VkDescriptorSet> sets{
	    get_hair_system()->get_global_ds(),
	    descriptor_set,
	    hair->descriptor_set,
	    grids->compute_descriptor_set,
	    hair->compute_descriptor_set,
	    colliders->descriptor_set,
	};

	_local_ubo.model = hair->model_matrix;
	_local_buffer->convert_and_update(_local_ubo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_integrate);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, sets.size(), sets.data(), 0, NULL);

	vkCmdDispatch(cmd, hair->numStrands / group_size + 1, 1, 1);

	//// inter barrier
	// VkBufferMemoryBarrier bufferBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
	// bufferBarrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
	// bufferBarrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
	// bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	// bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	// bufferBarrier.buffer              = hair->pos_buffer->get_handle();
	// bufferBarrier.offset              = 0;
	// bufferBarrier.size                = hair->pos_buffer->get_size();

	// vkCmdPipelineBarrier(
	//     cmd,
	//     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	//     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	//     VK_FLAGS_NONE,
	//     0, nullptr,
	//     1, &bufferBarrier,
	//     0, nullptr);

	//// dispatch
	// vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_constraint);

	// vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, sets.size(), sets.data(), 0, NULL);
	//// dispatch
	// vkCmdDispatch(cmd, hair->numStrands / group_size + 1, 1, 1);

	printf("dispatch\n");
}

bool StrandHair::on_gui_update(vkb::Drawer &drawer)
{
	bool res{false};
	if (drawer.header("configuration"))
	{
		if (drawer.checkbox("Debug view", &debug))
		{
			res = true;
		}
	}
	return res;
}
}        // namespace hair_system