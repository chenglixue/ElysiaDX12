#pragma once
#include "MeshRenderer.h"
#include "Transform.h"

namespace ElysiaRenderer
{
    class Entity
    {
    public:
        eastl::string name;
        ElysiaHelper::Transform transform;
        std::unique_ptr<MeshRenderer> pMeshRenderer;
        
    public:
        Entity(eastl::string n) : name(std::move(n)) {}
        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;
        
    private:
    
    };
}

