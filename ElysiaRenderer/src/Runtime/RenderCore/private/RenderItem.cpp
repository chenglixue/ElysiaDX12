#include "stdafx.h"
#include "../public/RenderItem.h"

#include "Runtime/RenderCore/public/MeshRenderer.h"

namespace ElysiaRenderer
{
    const ElysiaModel::LoadedMaterial& RenderItem::GetMaterial() const
    {
        if (pAssociatedEntity != nullptr && pAssociatedEntity->pMeshRenderer != nullptr)
            return pAssociatedEntity->pMeshRenderer->GetMaterial();
        return loadedMaterial;
    }
}