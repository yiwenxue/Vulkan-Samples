#include "model.h"
#include "render_system.h"
#include "util.h"
#include "vulkan/vulkan_core.h"

namespace hair_system
{

void GridStructPipeline::destroy()
{
	RenderPipeline::destroy();
}

void GridStructPipeline::prepare_pipeline()
{
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
	    vkb::initializers::pipeline_input_assembly_state_create_info(
	        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	        0,
	        VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterization_state =
	    vkb::initializers::pipeline_rasterization_state_create_info(
	        VK_POLYGON_MODE_FILL,
	        VK_CULL_MODE_NONE,
	        VK_FRONT_FACE_CLOCKWISE,
	        0);

	VkPipelineColorBlendAttachmentState blend_attachment_state =
	    vkb::initializers::pipeline_color_blend_attachment_state(
	        0xf,
	        VK_FALSE);

	blend_attachment_state.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend_attachment_state.blendEnable         = VK_TRUE;
	blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
	blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;
	blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	VkPipelineColorBlendStateCreateInfo color_blend_state =
	    vkb::initializers::pipeline_color_blend_state_create_info(
	        1,
	        &blend_attachment_state);
	color_blend_state.logicOp           = VK_LOGIC_OP_COPY;        // Optional
	color_blend_state.attachmentCount   = 1;
	color_blend_state.pAttachments      = &blend_attachment_state;
	color_blend_state.blendConstants[0] = 0.0f;        // Optional
	color_blend_state.blendConstants[1] = 0.0f;        // Optional
	color_blend_state.blendConstants[2] = 0.0f;        // Optional
	color_blend_state.blendConstants[3] = 0.0f;        // Optional

	// Note: Using Reversed depth-buffer for increased precision, so Greater depth values are kept
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
	    vkb::initializers::pipeline_depth_stencil_state_create_info(
	        VK_TRUE,
	        VK_FALSE,
	        VK_COMPARE_OP_GREATER);
	// depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	// depth_stencil_state.minDepthBounds        = 0.0f;
	// depth_stencil_state.maxDepthBounds        = 1.0f;
	// depth_stencil_state.stencilTestEnable     = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewport_state =
	    vkb::initializers::pipeline_viewport_state_create_info(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisample_state =
	    vkb::initializers::pipeline_multisample_state_create_info(
	        VK_SAMPLE_COUNT_1_BIT,
	        0);

	std::vector<VkDynamicState> dynamic_state_enables = {
	    VK_DYNAMIC_STATE_VIEWPORT,
	    VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamic_state =
	    vkb::initializers::pipeline_dynamic_state_create_info(
	        dynamic_state_enables.data(),
	        static_cast<uint32_t>(dynamic_state_enables.size()),
	        0);

	// Vertex bindings and attributes
	std::vector<VkVertexInputBindingDescription> binding_descriptions = {
	    vkb::initializers::vertex_input_binding_description(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	    vkb::initializers::vertex_input_binding_description(1, sizeof(GridCell), VK_VERTEX_INPUT_RATE_INSTANCE),
	};
	std::vector<VkVertexInputAttributeDescription> input_attributes = {
	    vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)),
	    vkb::initializers::vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, nor)),
	    vkb::initializers::vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)),
	    vkb::initializers::vertex_input_attribute_description(0, 3, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord)),
	    vkb::initializers::vertex_input_attribute_description(1, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GridCell, pos)),
	    vkb::initializers::vertex_input_attribute_description(1, 5, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(GridCell, scale)),
	};

	VkPipelineVertexInputStateCreateInfo input_state = vkb::initializers::pipeline_vertex_input_state_create_info();
	input_state.vertexBindingDescriptionCount        = static_cast<uint32_t>(binding_descriptions.size());
	input_state.pVertexBindingDescriptions           = binding_descriptions.data();
	input_state.vertexAttributeDescriptionCount      = static_cast<uint32_t>(input_attributes.size());
	input_state.pVertexAttributeDescriptions         = input_attributes.data();

	// Model pipeline
	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages;
	shader_stages[0] = get_renderer()->load_shader("hair_simulation/grid.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = get_renderer()->load_shader("hair_simulation/grid.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	auto render_pass = *get_renderer()->get_render_pass();

	VkGraphicsPipelineCreateInfo pipeline_create_info =
	    vkb::initializers::pipeline_create_info(
	        pipeline_layout,
	        render_pass,
	        0);

	pipeline_create_info.pVertexInputState   = &input_state;
	pipeline_create_info.pInputAssemblyState = &input_assembly_state;
	pipeline_create_info.pRasterizationState = &rasterization_state;
	pipeline_create_info.pColorBlendState    = &color_blend_state;
	pipeline_create_info.pMultisampleState   = &multisample_state;
	pipeline_create_info.pViewportState      = &viewport_state;
	pipeline_create_info.pDepthStencilState  = &depth_stencil_state;
	pipeline_create_info.pDynamicState       = &dynamic_state;
	pipeline_create_info.stageCount          = static_cast<uint32_t>(shader_stages.size());
	pipeline_create_info.pStages             = shader_stages.data();

	auto pipeline_cache = *get_renderer()->get_pipeline_cache();

	VK_CHECK(vkCreateGraphicsPipelines(get_device()->get_handle(), pipeline_cache, 1, &pipeline_create_info, nullptr, &pipeline));
}

void GridStructPipeline::prepare_texture()
{
}

void GridStructPipeline::prepare_uniform_buffer()
{}

std::vector<VkDescriptorPoolSize> GridStructPipeline::regist_pool_size()
{
	std::vector<VkDescriptorPoolSize> pool_sizes = {
	    // local descriptor set
	    vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
	};
	return pool_sizes;
}

void GridStructPipeline::prepare_descriptor_set_layout()
{
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
	};
	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(
	        set_layout_bindings.data(),
	        static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device()->get_handle(), &descriptor_layout, nullptr, &descriptor_set_layout));

	std::vector<VkDescriptorSetLayout> ds_layouts{
	    get_renderer()->get_global_ds_layouts(),
	    descriptor_set_layout,
	};

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = vkb::initializers::pipeline_layout_create_info(
	    ds_layouts.data(),
	    ds_layouts.size());

	VK_CHECK(vkCreatePipelineLayout(get_device()->get_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout));
}

void GridStructPipeline::prepare_descriptor_set()
{
}

void GridStructPipeline::draw(VkCommandBuffer cmd, GridStruct *grid)
{
	std::vector<VkDescriptorSet> sets{get_renderer()->get_descriptor_set(), grid->descriptor_set};

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, sets.size(), sets.data(), 0, NULL);

	VkDeviceSize offsets[1] = {0};

	auto *vertices_buffer = grid->grid_model->vertices_buffer.get();
	auto *indices_buffer  = grid->grid_model->indices_buffer.get();

	vkCmdBindVertexBuffers(cmd, 0, 1, vertices_buffer->get(), offsets);
	vkCmdBindVertexBuffers(cmd, 1, 1, grid->instance_buffer->get(), offsets);

	vkCmdBindIndexBuffer(cmd, indices_buffer->get_handle(), 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, grid->grid_model->indices.size(), grid->cells.size(), 0, 0, 0);
}

}        // namespace hair_system