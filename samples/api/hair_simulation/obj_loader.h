#pragma once
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

#include "vertex.h"
namespace hair_system
{
int load_obj(std::string filename, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);
}