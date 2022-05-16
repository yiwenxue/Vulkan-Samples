#include "scene.h"

namespace hair_system
{

void Scene::add_model(Model *model)
{
	models.push_back(model);
}

void Scene::add_hair(Hair *hair)
{
	hairs.push_back(hair);
}

void Scene::add_grid(GridStruct *grid)
{
	grids.push_back(grid);
}

void Scene::add_collider(ColliderPool *collider)
{
	colliders.push_back(collider);
}

// const std::vector<Model *>  &Scene::get_models() const{}
// const std::vector<Hair *>   &Scene::get_hairs() const{}
// const std::vector<Collider> &Scene::get_colliders() const{}
// const std::vector<GridCell> &get_grids() const{}

// vkb::core::Buffer Scene::get_time_buffer(){}

// vkb::core::Buffer Scene::get_model_buffer(){}

// vkb::core::Buffer Scene::get_hair_buffer(){}

// vkb::core::Buffer Scene::get_grid_buffer(){}

// vkb::core::Buffer get_collider_buffer(){}

void Scene::update_time()
{}

// sync data ?
void Scene::update()
{}

}        // namespace hair_system