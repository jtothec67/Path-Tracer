#include "Sphere.h"

#include <IMGUI/imgui.h>

Sphere::Sphere(std::string _name)
{
    mName = _name;
}

Sphere::Sphere(std::string _name, glm::vec3 _position)
{
    mName = _name;
    mPosition = _position;
}

Sphere::Sphere(std::string _name, glm::vec3 _position, float _radius)
{
    mName = _name;
    mPosition = _position;
    mRadius = _radius;
}

Sphere::Sphere(std::string _name, glm::vec3 _position, float _radius, glm::vec3 _albedo)
{
    mName = _name;
    mPosition = _position;
    mRadius = _radius;
    mMaterial.albedo = _albedo;
}


bool Sphere::RayIntersect(const Ray& _ray, float _tMin, float _tMax, Hit& _out)
{
    // Vector from sphere center to ray origin
    const glm::vec3 oc = _ray.origin - mPosition;

    // Quadratic coefficients for |oc + t*d|^2 = r^2  ->  a t^2 + 2h t + c = 0
    const float a = glm::dot(_ray.direction, _ray.direction);
    const float h = glm::dot(oc, _ray.direction); // Half-b
    const float c = glm::dot(oc, oc) - mRadius * mRadius;

    const float disc = h * h - a * c;
    if (disc < 0.0f) return false; // No real roots

    const float sqrtDisc = std::sqrt(disc);

    // Find nearest root in the acceptable range
    float t = (-h - sqrtDisc) / a; // Smaller root
    if (t < _tMin || t > _tMax)
    {
        t = (-h + sqrtDisc) / a;
        if (t < _tMin || t > _tMax) return false;
    }

    // Fill hit record
    _out.t = t;
    _out.p = _ray.origin + t * _ray.direction;

    // Outward normal (geometric)
    glm::vec3 outward = (_out.p - mPosition) / mRadius;

    // Face-forward the shading normal and record which side we hit
    bool frontFace = glm::dot(_ray.direction, outward) < 0.0f;
    _out.n = frontFace ? outward : -outward;

    _out.mat = &mMaterial;
    return true;
}

void Sphere::UpdateUI()
{
    if (ImGui::TreeNode(mName.c_str()))
    {
        ImGui::DragFloat3("Position ", &mPosition[0], 0.1);
        ImGui::DragFloat3("Rotation ", &mRotation[0], 1.0f);
        ImGui::SliderFloat("Radius ", &mRadius, 0.0f, 20.0f);
        ImGui::ColorEdit3("Albedo", &mMaterial.albedo.r);
        ImGui::DragFloat("Roughness", &mMaterial.roughness, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Metallic", &mMaterial.metallic, 0.01f, 0.0f, 1.0f);
        ImGui::ColorEdit3("Emission Colour", &mMaterial.emissionColour.r);
        ImGui::DragFloat("Emission Strength", &mMaterial.emissionStrength, 0.1f, 0.0f, 100.0f);
        ImGui::TreePop();
    }
}