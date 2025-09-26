#pragma once

#include "RayObject.h"

#include <GLM/glm.hpp>

class Box : public RayObject
{
public:
	Box(std::string _name);
	~Box() {}

	bool RayIntersect(const Ray& _ray, float _tMin, float _tMax, Hit& _out) override;

	void UpdateUI() override;

	void SetSize(const glm::vec3& _size) { mSize = _size; }
	glm::vec3 GetSize() { return mSize; }
	

private:
	glm::vec3 mSize = glm::vec3(1.0f);
};