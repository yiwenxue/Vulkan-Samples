#include "hybrid_hair.h"
#include "hair_system.h"

namespace hair_system
{
HybridHair::HybridHair(vkb::Device *device, HairSystem *hair_system) :
    HairMethod(device, hair_system, HairSimuMethod::HYBRID)
{
	prepare();
}

HybridHair::~HybridHair()
{
	// vkDestroyPipeline(get_hair_system()->get_device()->get_handle(), pipeline_integrate, nullptr);
	// vkDestroyPipeline(get_hair_system()->get_device()->get_handle(), pipeline_constraint, nullptr);

	vkDestroyDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), descriptor_set_layout, nullptr);
	vkDestroyPipelineLayout(get_hair_system()->get_device()->get_handle(), pipeline_layout, nullptr);
}

void HybridHair::prepare_compute_descriptor_pool()
{}

void HybridHair::prepare_compute_descriptor_set_layouts()
{
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    // Binding 0 : local uniform
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        0),
	    // Binding 1 : time uniform
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        1),
	    // Binding 1 : strand storage buffer
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        VK_SHADER_STAGE_COMPUTE_BIT,
	        2),
	};

	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_hair_system()->get_device()->get_handle(), &descriptor_layout, nullptr, &descriptor_set_layout));

	std::vector<VkDescriptorSetLayout> local_ds_layout, ds_layouts;

	ds_layouts = {get_hair_system()->get_global_ds_layout()};

	local_ds_layout = {descriptor_set_layout};

	std::copy(local_ds_layout.cbegin(), local_ds_layout.cend(), back_inserter(ds_layouts));

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = vkb::initializers::pipeline_layout_create_info(
	    ds_layouts.data(),
	    ds_layouts.size());

	VK_CHECK(vkCreatePipelineLayout(get_hair_system()->get_device()->get_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout));
}

void HybridHair::prepare_compute_descriptor_sets()
{
	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        *get_hair_system()->get_descriptor_pool(),
	        &descriptor_set_layout,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(get_hair_system()->get_device()->get_handle(), &alloc_info, &descriptor_set));
}

void HybridHair::prepare_compute_pipeline()
{}
void HybridHair::prepare_compute_uniform_buffers()
{}
void HybridHair::update_compute_uniform_buffers()
{}

bool HybridHair::prepare()
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

void HybridHair::simulate(VkCommandBuffer cmd, Scene *scene, float delta_time)
{
}

bool HybridHair::on_gui_update(vkb::Drawer &drawer)
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