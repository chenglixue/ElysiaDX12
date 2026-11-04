#include "private\ShadingCommon.hlsl"

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;
    
    float4 g_SourceSize;
    float4 g_TargetSize;
    
    UINT g_SourceTexIndex;
    UINT g_TargetTexIndex;
}

#define SPD_THREAD_GROUP [numthreads(SSSR_SPD_GROUP_SIZE, SSSR_SPD_GROUP_SIZE, 1)]

bool IsThreadOutWorkArea(UINT2 ThreadPos, float4 AreaSize) 
{
    BRANCH
    if (ThreadPos.x >= AreaSize.x || ThreadPos.y >= AreaSize.y || ThreadPos.x < 0 || ThreadPos.y < 0)
    {
        return true;
    }

    return false;
}

// --------------------------------------------------------------------------------------- Copy RT ---------------------------------------------------------------------------------------
void Elysia_Save_Copy_Tex(UINT2 WritePos, float Value)
{
    RWTexture2D<float> HIZTex = ResourceDescriptorHeap[g_TargetTexIndex];
    HIZTex[WritePos] = Value;
}

SPD_THREAD_GROUP
void DoCopyRT(UINT3 ID : SV_DispatchThreadID)
{
    UINT2 WritePos = ID.xy;
    UINT2 ReadPos = ID.xy;
    
    BRANCH
    if (IsThreadOutWorkArea(WritePos, g_TargetSize))
    {
        return;
    }
    
    float CameraDepth = LoadTexture2D(OpaqueDepthIndex, ReadPos);
    Elysia_Save_Copy_Tex(WritePos, CameraDepth);
}

// --------------------------------------------------------------------------------------- SPD ---------------------------------------------------------------------------------------
float GetMinDepth(float4 GatherDepth)
{
    float o = FLT_MAX;
    
    UNROLL
    for (UINT i = 0; i < 4; i++)
    {
        o = min(o, GatherDepth[i]);
    }
    
    return o;
}
void Elysia_Save_HIZ_Tex(UINT2 WritePos, float Value)
{
    RWTexture2D<float> HIZTex = ResourceDescriptorHeap[g_TargetTexIndex];
    HIZTex[WritePos] = Value;
}

SPD_THREAD_GROUP
void DoSPD(UINT3 ID : SV_DispatchThreadID)
{
    UINT2 WritePos = ID.xy;
    UINT2 ReadPos = ID.xy * 2;
    
    BRANCH
    if (IsThreadOutWorkArea(WritePos, g_TargetSize))
    {
        return;
    }
    
    const float2 SampleUV = (ReadPos + 1.f) * g_SourceSize.zw;
    const float4 GatherDepth = GatherRedTexture2D(g_SourceTexIndex, SampleUV, ClampPointSampler);
    const float MaxDepth = GetMinDepth(GatherDepth);
    
    Elysia_Save_HIZ_Tex(WritePos, MaxDepth);
}