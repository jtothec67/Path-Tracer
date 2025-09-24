#pragma once

#include "RayObject.h"

#include <GLM/glm.hpp>

class Sphere : public RayObject
{
public:
	Sphere(std::string _name);
	Sphere(std::string _name, glm::vec3 _position);
	Sphere(std::string _name, glm::vec3 _position, float _radius);
	Sphere(std::string _name, glm::vec3 _position, float _radius, glm::vec3 _albedo);
	~Sphere() {}

	bool RayIntersect(const Ray& _ray, float _tMin, float _tMax, Hit& _out) override;

	void UpdateUI() override;

	void SetRadius(float _radius) { mRadius = _radius; }
	float GetRadius() { return mRadius; }

private:
	float mRadius = 1.f;
};