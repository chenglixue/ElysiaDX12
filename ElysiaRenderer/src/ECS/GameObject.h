#pragma once
#include "MeshRenderer.h"
#include "Transform.h"

namespace ElysiaRenderer
{
    class GameObject
    {
    public:
        ElysiaHelper::Transform transform;
        std::unique_ptr<MeshRenderer> pMeshRenderer;
        
    public:
        
        
    private:
    
    };
}

