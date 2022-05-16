#pragma once

#include "define.h"
#include "model.h"

#include "api_vulkan_sample.h"

using namespace std::chrono;

namespace hair_system
{
class Scene
{
  public:
	Scene() = default;

	~Scene()
	{
		for (auto model : models)
		{
			if (model)
			{
				delete model;
			}
		}
		for (auto hair : hairs)
		{
			if (hair)
			{
				delete hair;
			}
		}
		for (auto grid : grids)
		{
			if (grid)
			{
				delete grid;
			}
		}
		for (auto collider : colliders)
		{
			if (collider)
			{
				delete collider;
			}
		}
	};

	void add_model(Model *model);

	void add_hair(Hair *hair);

	void add_grid(GridStruct *grid);

	void add_collider(ColliderPool *collider);

	const std::vector<Model *> &get_models() const
	{
		return models;
	}
	const std::vector<Hair *> &get_hairs() const
	{
		return hairs;
	}
	const std::vector<ColliderPool *> &get_colliders() const
	{
		return colliders;
	}
	const std::vector<GridStruct *> &get_grids() const
	{
		return grids;
	}

	vkb::core::Buffer *get_time_buffer()
	{
		return time_buffer.get();
	}

	vkb::core::Buffer *get_camera_buffer()
	{
		return global_buffer.get();
	}

	// VkBuffer get_model_buffer()
	// {
	// 	return global_buffer->get_handle();
	// }

	// VkBuffer get_hair_buffer()
	// {
	// 	return global_buffer->get_handle();
	// }

	// VkBuffer get_grid_buffer()
	// {
	// 	return global_buffer->get_handle();
	// }

	// VkBuffer get_collider_buffer()
	// {
	// 	return global_buffer->get_handle();
	// }

	void update_time();

	// sync data ?
	void update();

  protected:
  private:
	high_resolution_clock::time_point startTime = high_resolution_clock::now();

	// model
	std::vector<Model *> models;
	// collider
	std::vector<ColliderPool *> colliders;

	// hair
	std::vector<Hair *> hairs;
	// grid
	std::vector<GridStruct *> grids;

	std::unique_ptr<vkb::core::Buffer> time_buffer;

	std::unique_ptr<vkb::core::Buffer> global_buffer;
};
}        // namespace hair_system