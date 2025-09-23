#pragma once

#include "RayObject.h"

#include <vector>
#include <memory>

class PathTracer
{
public:
	glm::vec3 TraceRay(Ray _ray, glm::vec3 _camPos, int _depth);

	const std::vector<std::shared_ptr<RayObject>>& GetRayObjects() { return rayObjects; }
	void AddRayObject(std::shared_ptr<RayObject> _rayObject) { rayObjects.push_back(_rayObject); }

	// Scene management
	int GetSizeOfRayObjects() { return rayObjects.size(); }
	void ClearScene() { rayObjects.clear(); }

	void SetDepth(int _depth) { mMaxDepth = _depth; }
	int GetDepth() { return mMaxDepth; }

private:
	glm::vec3 mBackgroundColour{ 0.2f, 0.2f, 0.2f };
	int mMaxDepth = 2;

	std::vector<std::shared_ptr<RayObject>> rayObjects;
};