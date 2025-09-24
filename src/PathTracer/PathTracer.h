#pragma once

#include "RayObject.h"

#include <vector>
#include <memory>

class PathTracer
{
public:
	glm::vec3 TraceRay(Ray _ray, int _depth, bool _albedoOnly = false);

	const std::vector<std::shared_ptr<RayObject>>& GetRayObjects() { return rayObjects; }
	void AddRayObject(std::shared_ptr<RayObject> _rayObject) { rayObjects.push_back(_rayObject); }

	// Scene management
	int GetSizeOfRayObjects() { return rayObjects.size(); }
	void ClearScene() { rayObjects.clear(); }

private:
	glm::vec3 mBackgroundColour{ 0.5f, 0.5f, 0.5f };

	std::vector<std::shared_ptr<RayObject>> rayObjects;
};