#include "stdafx.h"
#include "GIUtility.h"

namespace ElysiaRenderer
{
    void DebugDumpTLASInstances(
        const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs,
        const std::vector<std::string>& instanceNames)
    {
        Log("======= DDGI TLAS Debug Dump =======");

        for (size_t i = 0; i < instanceDescs.size(); ++i)
        {
            const auto& desc = instanceDescs[i];
            const std::string& name = (i < instanceNames.size()) ? instanceNames[i] : "Unknown";

            // 打印实例基本信息
            Log("Instance [%d]: %s", i, name.c_str());
            Log(" - InstanceID: %d", desc.InstanceID);
            Log(" - InstanceMask: 0x%02X", desc.InstanceMask); // 重点检查是否为 0x00
            Log(" - Flags: 0x%X", desc.Flags);

            // 检查 Flags 逻辑
            if (desc.Flags & D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE)
                Log("   [Flag] Cull Disable: ON");
            if (desc.Flags & D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE)
                Log("   [Flag] FrontFace: CCW");

            // 打印变换矩阵的第一行，确认物体是否有极其离谱的缩放或位移
            Log(" - Transform Row0: [%.2f, %.2f, %.2f, %.2f]",
                desc.Transform[0][0],
                desc.Transform[0][1],
                desc.Transform[0][2],
                desc.Transform[0][3]);
            Log(" - Transform Row1: [%.2f, %.2f, %.2f, %.2f]",
                desc.Transform[1][0],
                desc.Transform[1][1],
                desc.Transform[1][2],
                desc.Transform[1][3]);
            Log(" - Transform Row2: [%.2f, %.2f, %.2f, %.2f]",
                desc.Transform[2][0],
                desc.Transform[2][1],
                desc.Transform[2][2],
                desc.Transform[2][3]);

            // 重点排查：如果掩码不包含 0xFF 且你的 TraceRay 使用了 0xFF，射线会漏掉它
            if (desc.InstanceMask == 0)
            {
                Log("   [WARNING] This instance has 0 mask and will ALWAYS be missed!");
            }

            Log("------------------------------------");
        }
    }
}