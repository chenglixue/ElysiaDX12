#include "stdafx.h"
#include "RenderHelper.h"

namespace ElysiaHelper
{
    Vector4 GetZBufferParams(float nearZ, float farZ)
    {
        return Vector4(
            1 - farZ / nearZ,
            farZ / nearZ,
            (1 - farZ / nearZ) / farZ,
            (farZ / nearZ) / farZ);
    }

    void SetName(ID3D12Object *pObj, const char * name)
    {
        if (name != NULL)
        {
            SetName(pObj, std::string(name));
        }
    }

    void SetName(ID3D12Object *pObj, const std::string &name)
    {
        assert(pObj != NULL);

        wchar_t NameBuffer[128];

        // Truncate the string if it's too big (keep the tail as it likely has the most useful information - some name have full paths)
        if (name.size() >= 128)
            swprintf(NameBuffer, 128, L"%S", name.substr(name.size() - 127, name.size()).c_str());
        else
            swprintf(NameBuffer, name.size()+1, L"%S", name.c_str());

        pObj->SetName(NameBuffer);
    }
}