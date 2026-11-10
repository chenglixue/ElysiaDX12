#include "private\ShadingCommon.hlsl"

#ifndef SSSR_COMPOSITE_GROUP_SIZE
#define SSSR_COMPOSITE_GROUP_SIZE 8
#endif

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewMatrix;
    Matrix projMatrix_I;

    float4 g_TargetSize;
    float g_Intensity;

    UINT g_IntersectionOutputTexIndex;
    UINT g_CameraColorUAV;
}

[numthreads(SSSR_COMPOSITE_GROUP_SIZE, SSSR_COMPOSITE_GROUP_SIZE, 1)]
void Composite(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_TargetSize.x || id.y >= g_TargetSize.y)
        return;

    float2 uv = ((float2)id.xy + 0.5f) * g_TargetSize.zw;
    float rawDepth = LoadTexture2D(OpaqueDepthIndex, int2(id.xy)).r;
    FDecodeGBufferData gbuffer = GetDecodeGBufferData(uv);

    float linear01 = Linear01Depth(rawDepth, g_ZBufferParams);
    if (gbuffer.ShadingModelID == Shading_Model_ID_Unlit || linear01 >= 0.999f)
        return;

    float4 reflection = LoadTexture2D(g_IntersectionOutputTexIndex, int2(id.xy));
    float confidence = saturate(reflection.a);
    if (confidence <= 0.0f)
        return;

    float3 positionVS = ComputeViewSpacePosition(uv, rawDepth, projMatrix_I);
    float3 viewDirVS = normalize(-positionVS);
    float3 normalVS = TransformNormalFromWSToVS(gbuffer.WorldNormal, viewMatrix).xyz;
    float NoV = saturate(dot(normalVS, viewDirVS));
    float3 fresnel = gbuffer.SpecularColor + (1.0f - gbuffer.SpecularColor) * pow(1.0f - NoV, 5.0f);

    // Intersection stores pre-exposed scene radiance at the hit (misses keep alpha 0,
    // so environment IBL already in Camera Color is not added a second time).
    float3 add = max(reflection.rgb, 0.0f) * fresnel * confidence * max(g_Intensity, 0.0f);

    RWTexture2D<float4> color = ResourceDescriptorHeap[g_CameraColorUAV];
    float4 scene = color[id.xy];
    scene.rgb += add;
    color[id.xy] = scene;
}
