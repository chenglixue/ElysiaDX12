#pragma once
#include "Runtime/RenderCore/MeshRenderer.h"
#include "Transform.h"
#include "Runtime/RenderCore/BufferManager.h"

namespace ElysiaEngine
{
    using namespace ElysiaRenderer;
    
    struct Entity
    {
    public:
        eastl::string name;
        Transform transform;
        std::unique_ptr<MeshRenderer> meshRenderer;

        Entity(eastl::string n) : name(std::move(n)) {}
        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;
    };
}
