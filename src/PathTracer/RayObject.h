#pragma once

#include "Ray.h"

#include <GLM/glm.hpp>

#include <string>
#include <vector>

struct Material
{
	glm::vec3 albedo = glm::vec3(1.0f);
	float roughness = 1.f;
	float metallic = 0.0f;
	glm::vec3 emissionColour = glm::vec3(1.0f);
	float emissionStrength = 0.0f;
};

struct Hit
{
	float t; // Distance along ray (ray(t) = o + t*d)
	glm::vec3 p; // Hit point
	glm::vec3 n; // Normal
	Material* mat;
};

class RayObject
{
public:
	RayObject() {}
	~RayObject() {}

	// Pure virtual functions for derived classes to implement
	virtual bool RayIntersect(const Ray& _ray, float _tMin, float _tMax, Hit& _out) = 0;

	void SetName(const std::string& _name) { mName = _name; }
	void GetName(std::string& _name) { _name = mName; }

	void SetPosition(const glm::vec3& _position) { mPosition = _position; }
	glm::vec3 GetPosition() { return mPosition; }

	void SetRotation(const glm::vec3& _rotation) { mRotation = _rotation; }
	glm::vec3 GetRotation() { return mRotation; }

	void SetMaterial(const Material& _material) { mMaterial = _material; }
	Material GetMaterial() { return mMaterial; }

	// UI function
	virtual void UpdateUI() {}

protected:
	std::string mName = "Object";

	glm::vec3 mPosition = glm::vec3(0.0f);
	glm::vec3 mRotation = glm::vec3(0.0f);

	Material mMaterial;
};