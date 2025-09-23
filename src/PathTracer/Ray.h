#pragma once

#include <GLM/glm.hpp>

struct Ray
{
	Ray() : origin(glm::vec3(0)), direction(glm::vec3(0)) {}
	Ray(glm::vec3 _origin, glm::vec3 _direction) : origin(_origin), direction(glm::normalize(_direction)) {}
	glm::vec3 origin;
	glm::vec3 direction;
};