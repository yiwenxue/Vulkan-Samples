/* Copyright (c) 2019-2020, Sascha Willems
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

/*
 * Compute shader N-body simulation using two passes and shared compute shader memory
 */

#pragma once

#include "api_vulkan_sample.h"

#include "scene.h"

#include "hair_system.h"
#include "hybrid_hair.h"
#include "strand_hair.h"

#include "render_system.h"
#include "vulkan/vulkan_core.h"

#if defined(__ANDROID__)
// Lower particle count on Android for performance reasons
#	define PARTICLES_PER_ATTRACTOR 3 * 1024
#else
#	define PARTICLES_PER_ATTRACTOR 4 * 1024
#endif

namespace hair_system
{
class HairSimulation : public ApiVulkanSample
{
  public:
	int32_t                     hair_method{0};
	std::vector<HairSystem *>   hair_systems{};
	std::vector<std::string>    hair_system_name{};
	std::unique_ptr<Renderer>   render_system;
	std::unique_ptr<HairSystem> hair_system;

	bool draw_grid{false};
	bool draw_head{true};
	bool draw_hair{true};
	bool pause_simulation{false};

	HairUBO hair_data{0.01, 0.01, -1.0, 1.0};
	HairUBO ori_hair_data{0.01, 0.01, -1.0, 1.0};

	std::unique_ptr<GeomPipeline>       geom_render;
	std::unique_ptr<HairPipeline>       hair_render;
	std::unique_ptr<GridStructPipeline> grid_render;

	std::unique_ptr<HybridHair> hybrid_simulator;
	std::unique_ptr<StrandHair> strand_simulator;

	std::unique_ptr<Scene> scene;

	HairSimulation();
	~HairSimulation();

	void setup_descriptor_pool();
	void create_semaphore();

	void draw();
	void simulate(float delta_time);

	void prepare_compute_command_buffer();

	void         build_command_buffers() override;
	void         build_simulate_command_buffers();
	virtual bool prepare(vkb::Platform &platform) override;

	virtual void update_uniform_buffers();
	virtual void update_simulation_parameters();

	virtual void render(float delta_time) override;
	virtual void resize(const uint32_t width, const uint32_t height) override;
	virtual void on_update_ui_overlay(vkb::Drawer &drawer) override;
	virtual void request_gpu_features(vkb::PhysicalDevice &gpu) override;

	virtual const std::vector<const char *> get_validation_layers() override;

  private:
	VkSubmitInfo compute_submit_info;

	uint32_t _graphic_queue_family;
	uint32_t _compute_queue_family;
	VkQueue  _compute_queue;

	VkSemaphore _graphic_semaphore;
	VkSemaphore _compute_semaphore;

	VkCommandPool   _compute_command_pool;
	VkCommandBuffer _compute_command_buffer;
};
}        // namespace hair_system
std::unique_ptr<vkb::Application> create_hair_simulation();
