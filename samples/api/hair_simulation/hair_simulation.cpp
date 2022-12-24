/* Copyright (c) 2020, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hair_simulation.h"
#include "glm/detail/type_mat.hpp"
#include "render_system.h"
#include "vulkan/vulkan_core.h"

#include "hybrid_hair.h"
#include "model.h"
#include "obj_loader.h"
#include "strand_hair.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace hair_system
{
HairSimulation::HairSimulation()
{
	title       = "Compute shader Hair simulation";
	camera.type = vkb::CameraType::LookAt;

	// Note: Using Reversed depth-buffer for increased precision, so Z-Near and Z-Far are flipped
	camera.set_perspective(60.0f, (float) width / (float) height, 512.0f, 0.1f);
	camera.set_rotation(glm::vec3(-26.0f, 75.0f, 0.0f));
	camera.set_translation(glm::vec3(0.0f, 0.0f, -14.0f));
	camera.translation_speed = 2.5f;

	add_instance_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	add_device_extension(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);
}

HairSimulation::~HairSimulation()
{
	if (device)
	{
		vkDestroyCommandPool(device->get_handle(), _compute_command_pool, nullptr);
		vkDestroySemaphore(device->get_handle(), _graphic_semaphore, nullptr);
		vkDestroySemaphore(device->get_handle(), _compute_semaphore, nullptr);
	}
	geom_render.reset();
	hair_render.reset();
	render_system.reset();

	hybrid_simulator.reset();
	strand_simulator.reset();
	hair_system.reset();
	scene.reset();
}

void HairSimulation::request_gpu_features(vkb::PhysicalDevice &gpu)
{
	// Enable anisotropic filtering if supported
	if (gpu.get_features().samplerAnisotropy)
	{
		gpu.get_mutable_requested_features().samplerAnisotropy = VK_TRUE;
	}
	if (gpu.get_features().fillModeNonSolid)
	{
		gpu.get_mutable_requested_features().fillModeNonSolid = VK_TRUE;
	}
}

void HairSimulation::build_command_buffers()
{
	// Destroy command buffers if already present
	if (!check_command_buffers())
	{
		destroy_command_buffers();
		create_command_buffers();
	}

	VkCommandBufferBeginInfo command_buffer_begin_info = vkb::initializers::command_buffer_begin_info();

	VkClearValue clear_values[2];
	clear_values[0].color        = {{0.8f, 0.8f, 0.8f, 1.f}};
	clear_values[1].depthStencil = {0.0f, 0};

	VkRenderPassBeginInfo render_pass_begin_info    = vkb::initializers::render_pass_begin_info();
	render_pass_begin_info.renderPass               = render_pass;
	render_pass_begin_info.clearValueCount          = 2;
	render_pass_begin_info.renderArea.extent.width  = width;
	render_pass_begin_info.renderArea.extent.height = height;
	render_pass_begin_info.pClearValues             = clear_values;

	VkViewport viewport = vkb::initializers::viewport((float) width, (float) height, 0.0f, 1.0f);
	VkRect2D   scissor  = vkb::initializers::rect2D(width, height, 0, 0);

	for (int32_t i = 0; i < draw_cmd_buffers.size(); ++i)
	{
		VK_CHECK(vkBeginCommandBuffer(draw_cmd_buffers[i], &command_buffer_begin_info));

		// Set target frame buffer
		render_pass_begin_info.framebuffer = framebuffers[i];
		vkCmdBeginRenderPass(draw_cmd_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(draw_cmd_buffers[i], 0, 1, &viewport);
		vkCmdSetScissor(draw_cmd_buffers[i], 0, 1, &scissor);

		auto models = scene->get_models();
		auto hairs  = scene->get_hairs();
		auto grids  = scene->get_grids();

		if (draw_head)
		{
			for (auto &model : models)
			{
				geom_render->draw(draw_cmd_buffers[i], model);
			}
		}

		if (draw_hair)
		{
			for (auto &hair : hairs)
			{
				hair_render->draw(draw_cmd_buffers[i], hair);
			}
		}

		if (draw_grid)
		{
			for (auto &grid : grids)
			{
				grid_render->draw(draw_cmd_buffers[i], grid);
			}
		}

		draw_ui(draw_cmd_buffers[i]);

		vkCmdEndRenderPass(draw_cmd_buffers[i]);

		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffers[i]));
	}
}

void HairSimulation::create_semaphore()
{
	VkSemaphoreCreateInfo semaphore_create_info = vkb::initializers::semaphore_create_info();

	VK_CHECK(vkCreateSemaphore(get_device().get_handle(), &semaphore_create_info, nullptr, &_graphic_semaphore));

	VK_CHECK(vkCreateSemaphore(get_device().get_handle(), &semaphore_create_info, nullptr, &_compute_semaphore));

	VkSubmitInfo submit_info         = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores    = &_compute_semaphore;
	VK_CHECK(vkQueueSubmit(_compute_queue, 1, &submit_info, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(_compute_queue));
}

void HairSimulation::build_simulate_command_buffers()
{
	VkCommandBufferBeginInfo command_buffer_begin_info = vkb::initializers::command_buffer_begin_info();

	VK_CHECK(vkBeginCommandBuffer(_compute_command_buffer, &command_buffer_begin_info));

	if (_compute_queue_family != _graphic_queue_family)
	{
		// create barrier
	}

	// First pass: integrate
	// -------------------------------------------------------------------------------------------------------
	strand_simulator->simulate(_compute_command_buffer, scene.get(), 0.1);
	// Second pass: integrate
	// -------------------------------------------------------------------------------------------------------

	// hair_systems[hair_method]->build_compute_command_buffers();

	vkEndCommandBuffer(_compute_command_buffer);
}

void HairSimulation::prepare_compute_command_buffer()
{
	_compute_queue_family = get_device().get_queue_family_index(VK_QUEUE_COMPUTE_BIT);
	_graphic_queue_family = get_device().get_queue_family_index(VK_QUEUE_GRAPHICS_BIT);

	vkGetDeviceQueue(get_device().get_handle(), _compute_queue_family, 0, &_compute_queue);

	VkCommandPoolCreateInfo command_pool_create_info = {};
	command_pool_create_info.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex        = get_device().get_queue_family_index(VK_QUEUE_COMPUTE_BIT);
	command_pool_create_info.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK(vkCreateCommandPool(get_device().get_handle(), &command_pool_create_info, nullptr, &_compute_command_pool));

	VkCommandBufferAllocateInfo command_buffer_allocate_info =
	    vkb::initializers::command_buffer_allocate_info(
	        _compute_command_pool,
	        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	        1);

	VK_CHECK(vkAllocateCommandBuffers(get_device().get_handle(), &command_buffer_allocate_info, &_compute_command_buffer));
}

void HairSimulation::setup_descriptor_pool()
{
	std::vector<VkDescriptorPoolSize> pool_sizes =
	    {
	        vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 30),
	        vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 30),
	        vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 30),
	    };

	VkDescriptorPoolCreateInfo descriptor_pool_create_info =
	    vkb::initializers::descriptor_pool_create_info(
	        static_cast<uint32_t>(pool_sizes.size()),
	        pool_sizes.data(),
	        40);

	VK_CHECK(vkCreateDescriptorPool(get_device().get_handle(), &descriptor_pool_create_info, nullptr, &descriptor_pool));
}

void HairSimulation::update_uniform_buffers()
{
	auto &_global_ubo = render_system->get_global_ubo();

	_global_ubo.view       = camera.matrices.view;
	_global_ubo.projection = camera.matrices.perspective;

	_global_ubo.projection[1][1] *= -1.0;

	render_system->update_uniform_buffers();
}

void HairSimulation::draw()
{
	ApiVulkanSample::prepare_frame();

	std::vector<VkPipelineStageFlags> graphics_wait_stage_masks;
	std::vector<VkSemaphore>          graphics_wait_semaphores;
	std::vector<VkSemaphore>          graphics_signal_semaphores;
	if (pause_simulation)
	{
		graphics_wait_stage_masks  = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		graphics_wait_semaphores   = {semaphores.acquired_image_ready};
		graphics_signal_semaphores = {semaphores.render_complete};
	}
	else
	{
		graphics_wait_stage_masks  = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		graphics_wait_semaphores   = {_compute_semaphore, semaphores.acquired_image_ready};
		graphics_signal_semaphores = {_graphic_semaphore, semaphores.render_complete};
	}

	submit_info.commandBufferCount   = 1;
	submit_info.pCommandBuffers      = &draw_cmd_buffers[current_buffer];
	submit_info.pWaitDstStageMask    = graphics_wait_stage_masks.data();
	submit_info.waitSemaphoreCount   = graphics_wait_semaphores.size();
	submit_info.pWaitSemaphores      = graphics_wait_semaphores.data();
	submit_info.signalSemaphoreCount = graphics_signal_semaphores.size();
	submit_info.pSignalSemaphores    = graphics_signal_semaphores.data();

	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
	ApiVulkanSample::submit_frame();
}

bool HairSimulation::prepare(vkb::Platform &platform)
{
	if (!ApiVulkanSample::prepare(platform))
	{
		return false;
	}

	setup_descriptor_pool();

	prepare_compute_command_buffer();

	// initialize render system
	render_system = std::make_unique<Renderer>(&get_device());
	render_system->set_render_pass(&render_pass);
	render_system->set_pipeline_cache(&pipeline_cache);
	render_system->set_descriptor_pool(&descriptor_pool);
	render_system->prepare();

	// initialize render methods
	geom_render = std::make_unique<GeomPipeline>(&get_device(), render_system.get());
	hair_render = std::make_unique<HairPipeline>(&get_device(), render_system.get());
	grid_render = std::make_unique<GridStructPipeline>(&get_device(), render_system.get());

	hair_system = std::make_unique<HairSystem>(&get_device());
	hair_system->set_pipeline_cache(&pipeline_cache);
	hair_system->set_descriptor_pool(&descriptor_pool);
	hair_system->prepare();

	hybrid_simulator = std::make_unique<HybridHair>(&get_device(), hair_system.get());
	strand_simulator = std::make_unique<StrandHair>(&get_device(), hair_system.get());

	// initialize scene mamger
	scene = std::make_unique<Scene>();

	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;

	// load assets head assets
	load_obj("scenes/mannequin_segment.obj", vertices, indices);
	auto *mannequin_segment = new Model(device.get(), descriptor_pool, vertices, indices);
	mannequin_segment->set_texture(load_texture("textures/mannequin_specular.png"));
	mannequin_segment->model_matrix = glm::mat4(1.0);
	mannequin_segment->local_ubo->convert_and_update(mannequin_segment->model_matrix);

	load_obj("scenes/mannequin.obj", vertices, indices);
	auto *mannequin = new Model(device.get(), descriptor_pool, vertices, indices);
	mannequin->set_texture(load_texture("textures/head.png"));
	mannequin->model_matrix = glm::mat4(1.0);
	mannequin->local_ubo->convert_and_update(mannequin->model_matrix);

	// load_obj("scenes/collisionTest.obj", vertices, indices);
	// auto *collisionTest = new Model(device.get(), descriptor_pool, vertices, indices);
	// collisionTest->set_texture(load_texture("textures/mannequin_specular.png"));
	// glm::vec3 trans             = glm::vec3(0.0, 2.8, -0.02);
	// glm::vec3 rot               = glm::vec3(-38.270, 0.0, 0.0);
	// glm::vec3 scale             = glm::vec3(0.817, 1.158, 1.1);
	// glm::mat4 transMat          = glm::translate(trans);
	// glm::mat4 rotXMat           = glm::rotate(rot.x * (float) (180.0 / M_PI), glm::vec3(1.0, 0.0, 0.0));
	// glm::mat4 rotYMat           = glm::rotate(rot.y * (float) (180.0 / M_PI), glm::vec3(0.0, 1.0, 0.0));
	// glm::mat4 rotZMat           = glm::rotate(rot.z * (float) (180.0 / M_PI), glm::vec3(0.0, 0.0, 1.0));
	// glm::mat4 scaleMat          = glm::scale(scale);
	// collisionTest->model_matrix = transMat * rotZMat * rotYMat * rotXMat * scaleMat * glm::translate(glm::vec3(-2.0, 0.0, -1.0));
	// collisionTest->local_ubo->convert_and_update(collisionTest->model_matrix);

	// scene->add_model(collisionTest);

	scene->add_model(mannequin_segment);
	scene->add_model(mannequin);

	// load hair assets
	auto *hair = new Hair(device.get(), descriptor_pool, "scenes/mannequin_segment.obj");
	scene->add_hair(hair);

	auto *grid = new GridStruct(device.get(), descriptor_pool, glm::vec3(0.0, 2.0, 0.0), glm::vec3(8.0), glm::ivec3(50));
	scene->add_grid(grid);

	Collider sphereCollider    = Collider(glm::vec3(2.0, 0.0, 1.0), glm::vec3(0.0), glm::vec3(1.0));
	Collider faceCollider      = Collider(glm::vec3(0.0, 2.511, 0.915), glm::vec3(0, 0.0, 0.0), glm::vec3(0.561, 0.749, 0.615));
	Collider headCollider      = Collider(glm::vec3(0.0, 2.7, -0.02), glm::vec3(-38.270, 0.0, 0.0), glm::vec3(0.84, 1.158, 1.2));
	Collider neckCollider      = Collider(glm::vec3(0.0, 1.35, -0.288), glm::vec3(18.301, 0.0, 0.0), glm::vec3(0.457, 1.0, 0.538));
	Collider bustCollider      = Collider(glm::vec3(0.0, -0.380, -0.116), glm::vec3(-17.260, 0.0, 0.0), glm::vec3(1.078, 1.683, 0.974));
	Collider shoulderRCollider = Collider(glm::vec3(-0.698, 0.087, -0.36), glm::vec3(-20.254, 13.144, 34.5), glm::vec3(0.721, 1.0, 0.724));
	Collider shoulderLCollider = Collider(glm::vec3(0.698, 0.087, -0.36), glm::vec3(-20.254, 13.144, -34.5), glm::vec3(0.721, 1.0, 0.724));

	std::vector<Collider> colliders{
	    sphereCollider, faceCollider, headCollider, neckCollider, bustCollider, shoulderRCollider, shoulderLCollider};

	auto *collider_pool = new ColliderPool(device.get(), descriptor_pool, colliders);
	scene->add_collider(collider_pool);

	hair_system_name = {"Strand Method", "Hybrid Method"};
	hair_style_name  = {"Straight hair", "Curly hair"};

	create_semaphore();

	build_command_buffers();
	build_simulate_command_buffers();

	prepared = true;
	return true;
}

void HairSimulation::render(float deltatime)
{
	if (!prepared)
		return;
	draw();
	if (!pause_simulation)
	{
		simulate(deltatime);
	}
	if (camera.updated)
	{
		update_uniform_buffers();
	}
}

void HairSimulation::simulate(float delta_time)
{
	auto &time_ubo      = hair_system->get_time_ubo();
	time_ubo.delta_time = delta_time;
	time_ubo.total_time += delta_time;

	hair_system->update_uniform_buffers();

	VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	// Submit compute commands
	VkSubmitInfo compute_submit_info         = vkb::initializers::submit_info();
	compute_submit_info.commandBufferCount   = 1;
	compute_submit_info.pCommandBuffers      = &_compute_command_buffer;
	compute_submit_info.waitSemaphoreCount   = 1;
	compute_submit_info.pWaitSemaphores      = &_graphic_semaphore;
	compute_submit_info.pWaitDstStageMask    = &wait_stage_mask;
	compute_submit_info.signalSemaphoreCount = 1;
	compute_submit_info.pSignalSemaphores    = &_compute_semaphore;
	VK_CHECK(vkQueueSubmit(_compute_queue, 1, &compute_submit_info, VK_NULL_HANDLE));
}

void HairSimulation::update_simulation_parameters()
{
	auto &hairs = scene->get_hairs();
	for (auto *hair : hairs)
	{
		hair->hair_data.damping       = hair_data.damping;
		hair->hair_data.friction      = hair_data.friction;
		hair->hair_data.stiffness     = hair_data.stiffness;
		hair->hair_data.gravityfactor = hair_data.gravityfactor;
		hair->hair_data.timefactor    = hair_data.timefactor;
		hair->hair_data.windspeed     = hair_data.windspeed;
		hair->hair_data.volume        = hair_data.volume;

		hair->updateUBO();
	}
}

void HairSimulation::on_update_ui_overlay(vkb::Drawer &drawer)
{
	if (drawer.header("Settings"))
	{
		drawer.text("simulate time: %fs", hair_system->get_time_ubo().total_time);

		if (drawer.combo_box("Method", &hair_method, hair_system_name))
		{
		}

		if (drawer.combo_box("Hair style", &hair_style, hair_style_name))
		{
			auto &hairs = scene->get_hairs();

			if (hair_style == 0)
			{
				for (auto *hair : hairs)
				{
					hair->genStraightHair();
					hair->reset();
				}
			}
			else
			{
				for (auto *hair : hairs)
				{
					hair->genCurlyHair();
					hair->reset();
				}
			}
			update_simulation_parameters();
		}

		if (drawer.button("reset"))
		{
			auto &hairs = scene->get_hairs();
			for (auto *hair : hairs)
			{
				hair->reset();
			}

			update_simulation_parameters();
		}

		drawer.checkbox("pause", &pause_simulation);

		if (drawer.checkbox("draw head", &draw_head))
		{
			build_command_buffers();
		}

		if (drawer.checkbox("draw hair", &draw_hair))
		{
			build_command_buffers();
		}

		if (drawer.checkbox("draw grid", &draw_grid))
		{
			build_command_buffers();
		}

		if (drawer.slider_float("Damping", &hair_data.damping, 0.0, 1.0))
		{
			update_simulation_parameters();
		}

		if (drawer.slider_float("firction", &hair_data.friction, 0.0, 1.0))
		{
			update_simulation_parameters();
		}

		if (drawer.slider_float("Volume preserve", &hair_data.volume, 0.0, 1.0))
		{
			update_simulation_parameters();
		}

		if (drawer.slider_float("Stiffness", &hair_data.stiffness, 0.0, 1.0))
		{
			update_simulation_parameters();
		}

		if (drawer.slider_float("Gravity factor", &hair_data.gravityfactor, -10.0, 10.0))
		{
			update_simulation_parameters();
		}

		if (drawer.slider_float("Substep", &hair_data.timefactor, 0.001, 1.0))
		{
			update_simulation_parameters();
		}

		if (drawer.slider_float("Wind Speed", &hair_data.windspeed, 0.0, 10.0))
		{
			update_simulation_parameters();
		}
	}
}

void HairSimulation::resize(const uint32_t width, const uint32_t height)
{
	ApiVulkanSample::resize(width, height);
}

const std::vector<const char *> HairSimulation::get_validation_layers()
{
	return {"VK_LAYER_KHRONOS_validation"};
}
}        // namespace hair_system
std::unique_ptr<vkb::Application> create_hair_simulation()
{
	return std::make_unique<hair_system::HairSimulation>();
}