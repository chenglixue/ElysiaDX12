#include "private\ShadingCommon.hlsl"
#include "private/SSSR/SSSRCommon.hlsli"

#ifndef SSSR_INTERSECT_GROUP_SIZE
#define SSSR_INTERSECT_GROUP_SIZE 8
#endif

#define SSSR_INTERSECT_THREAD_COUNT (SSSR_INTERSECT_GROUP_SIZE * SSSR_INTERSECT_GROUP_SIZE)
#define SSSR_INTERSECT_THREAD_GROUP [numthreads(SSSR_INTERSECT_GROUP_SIZE, SSSR_INTERSECT_GROUP_SIZE, 1)]

cbuffer PassConstant : register(b0, perPassSpace)
{
    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    float4 g_TargetSize;
    float g_RoughnessThreshold;
    uint g_SamplesPerQuad;
    UINT g_MostDetailedMip;
    UINT g_MaxRayTrace;
    float g_Thickness;

    UINT g_RayCounterBufferIndex;
    UINT g_RayListBufferIndex;
    UINT g_IntersectionOutputTexIndex;
    UINT g_IntersectionArgsBufferIndex;
    UINT g_HIZTexIndex;
    UINT g_CameraColorTexIndex;

    UINT g_SobolNoiseTexIndex;
}

UINT GetRayCount()
{
    RWStructuredBuffer<UINT> RayCounterBuffer = ResourceDescriptorHeap[g_RayCounterBufferIndex];
    return RayCounterBuffer[1];
}

UINT GetPackRayData(UINT RayIndex)
{
    RWStructuredBuffer<UINT> RayListBuffer = ResourceDescriptorHeap[g_RayListBufferIndex];
    return RayListBuffer[RayIndex];
}

void GetUnpackRayData(UINT PackData, out UINT2 SamplePos, out bool bCopyHorizontal, out bool bCopyVertical, out bool bCopyDiagonal)
{
    UnpackRayData(PackData, SamplePos, bCopyHorizontal, bCopyVertical, bCopyDiagonal);
}

float GetRoughness(UINT2 LoadPos)
{
    return LoadTexture2D(GBuffer1Index, LoadPos).b;
}

float GetSceneDepth(UINT2 LoadPos, UINT MipmapLevel)
{
    return LoadTexture2D(g_HIZTexIndex, LoadPos, MipmapLevel).r;
}

float3 LoadSSSRNormalWS(UINT2 SamplePos)
{
    float3 encodeNormalWS = LoadTexture2D(GBuffer3Index, SamplePos).xyz;
    return normalize(DecodeNormal(encodeNormalWS));
}

// http://jcgt.org/published/0007/04/01/paper.pdf by Eric Heitz
float3 SampleGGXVNDF(float3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
    float3 Vh = normalize(float3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lensq > 0 ? float3(-Vh.y, Vh.x, 0) * rsqrt(lensq) : float3(1, 0, 0);
    float3 T2 = cross(Vh, T1);
    float r = sqrt(U1);
    float phi = 2.0 * PI * U2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    float3 Ne = normalize(float3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
    return Ne;
}

float3 Sample_GGX_VNDF_Hemisphere(float3 Ve, float alpha, float U1, float U2)
{
    return SampleGGXVNDF(Ve, alpha, alpha, U1, U2);
}

float2 SampleRandomVector2D(uint2 pixel)
{
    return LoadTexture2D(g_SobolNoiseTexIndex, pixel.xy % 128).xy;
}

float3x3 GetTBN(float3 Normal)
{
    float3 U;
    BRANCH
    if (abs(Normal.z) > 0.0)
    {
        float k = sqrt(Normal.y * Normal.y + Normal.z * Normal.z);
        U.x = 0.0; U.y = -Normal.z / k; U.z = Normal.y / k;
    }
    else
    {
        float k = sqrt(Normal.x * Normal.x + Normal.y * Normal.y);
        U.x = Normal.y / k; U.y = -Normal.x / k; U.z = 0.0;
    }

    float3x3 TBN;
    TBN[0] = U;
    TBN[1] = cross(Normal, U);
    TBN[2] = Normal;
    return transpose(TBN);
}

float3 SampleReflectionVector(UINT2 SamplePos, float3 ViewDirVS, float3 NormalVS, float Roughness)
{
    // GetTBN() returns columns T, B, N. View -> tangent is mul(v, TBN).
    float3x3 TBN = GetTBN(NormalVS);
    float3 ViewDirTBN = mul(-ViewDirVS, TBN);
    ViewDirTBN.z = max(ViewDirTBN.z, 1e-6f);

    float2 Noise = SampleRandomVector2D(SamplePos);
    float alpha = max(Roughness * Roughness, 0.001f);
    float3 NormalTBN = Sample_GGX_VNDF_Hemisphere(ViewDirTBN, alpha, Noise.x, Noise.y);
    float3 ReflectDirTBN = reflect(-ViewDirTBN, NormalTBN);
    float3 ReflectDirVS = normalize(mul(TBN, ReflectDirTBN));

    // Keep the ray above the geometric surface.
    if (dot(ReflectDirVS, NormalVS) <= 0.0f)
        ReflectDirVS = normalize(reflect(ViewDirVS, NormalVS));

    return ReflectDirVS;
}

// Row-vector projection, matching ComputeViewSpacePosition / the rest of the engine.
float3 SSSR_ProjectPosition(float3 origin, float4x4 mat)
{
    float4 projected = mul(float4(origin, 1.0f), mat);
    projected.xyz /= max(projected.w, 1e-6f);
    projected.xy = 0.5f * projected.xy + 0.5f;
    projected.y = 1.0f - projected.y;
    return projected.xyz;
}

float3 SSSR_ProjectDirection(float3 origin, float3 direction, float3 screenSpaceOrigin, float4x4 mat)
{
    return SSSR_ProjectPosition(origin + direction, mat) - screenSpaceOrigin;
}

// Mip-0 / full-resolution depth march with a view-space thickness test.
// Hierarchical Hi-Z traversal is intentionally out of scope.
float3 RayMarch(float3 StartPosUV, float3 ReflectDirUV, float2 ViewSize, out bool bHit)
{
    bHit = false;

    float2 dirXY = ReflectDirUV.xy;
    float dirLen = length(dirXY);
    if (dirLen < 1e-6f)
        return StartPosUV;

    // DDA: one step moves exactly 1 pixel on the dominant screen axis.
    float2 dirPixels = dirXY * ViewSize;
    float maxPixel = max(max(abs(dirPixels.x), abs(dirPixels.y)), 1e-6f);
    float3 stepUV = ReflectDirUV / maxPixel;

    // Skip the starting texel so the ray does not immediately hit itself.
    float3 prevPos = StartPosUV;
    float3 currPos = StartPosUV + stepUV * 2.0f;

    const uint maxSteps = max(g_MaxRayTrace, 1u);
    UINT i = 0;
    for (; i < maxSteps; ++i)
    {
        if (any(currPos.xy < 0.0f) || any(currPos.xy > 1.0f))
            break;

        int2 samplePos = int2(ViewSize * currPos.xy);
        float surfaceZ = GetSceneDepth(samplePos, 0);
        float surfaceLinear01 = Linear01Depth(surfaceZ, g_ZBufferParams);
        if (surfaceLinear01 >= 0.999f)
        {
            prevPos = currPos;
            currPos += stepUV;
            continue;
        }

        float prevEye = LinearEyeDepth(prevPos.z, g_ZBufferParams);
        float rayEye = LinearEyeDepth(currPos.z, g_ZBufferParams);
        float surfaceEye = LinearEyeDepth(surfaceZ, g_ZBufferParams);
        float thickness = max(g_Thickness, 0.001f);

        bool wasInFront = prevEye < surfaceEye;
        bool nowBehind = rayEye >= surfaceEye;
        bool withinThickness = (rayEye - surfaceEye) <= thickness;
        if (wasInFront && nowBehind && withinThickness)
        {
            bHit = true;
            return currPos;
        }

        prevPos = currPos;
        currPos += stepUV;
    }

    return currPos;
}

float SSSR_ValidateHit(float3 hit, float2 uv, float3 worldSpaceRayDirection, float2 screenSize, float depthBufferThickness)
{
    if (any(hit.xy < 0.0f) || any(hit.xy > 1.0f))
        return 0.0f;

    float2 manhattanDist = abs(hit.xy - uv);
    if (all(manhattanDist < (2.0f / screenSize)))
        return 0.0f;

    int2 samplePos = int2(screenSize * hit.xy);
    float surfaceZ = GetSceneDepth(samplePos, 0);
    if (Linear01Depth(surfaceZ, g_ZBufferParams) >= 0.999f)
        return 0.0f;

    float3 hitNormal = LoadSSSRNormalWS(samplePos);
    if (dot(hitNormal, worldSpaceRayDirection) > 0.0f)
        return 0.0f;

    float3 viewSpaceSurface = ComputeViewSpacePosition(hit.xy, surfaceZ, projMatrix_I);
    float3 viewSpaceHit = ComputeViewSpacePosition(hit.xy, hit.z, projMatrix_I);
    float distance = length(viewSpaceSurface - viewSpaceHit);

    float2 fov = 0.05f * float2(screenSize.y / max(screenSize.x, 1.0f), 1.0f);
    float2 border = smoothstep(0.0f, fov, hit.xy) * (1.0f - smoothstep(1.0f - fov, 1.0f, hit.xy));
    float vignette = border.x * border.y;

    float confidence = 1.0f - smoothstep(0.0f, max(depthBufferThickness, 0.001f), distance);
    confidence *= confidence;
    return vignette * confidence;
}

SSSR_INTERSECT_THREAD_GROUP
void DoIntersect(UINT3 GroupPos : SV_GroupID,
    UINT3 ThreadPosInGroup : SV_GroupThreadID,
    UINT GroupIndex : SV_GroupIndex,
    UINT3 GlobalPos : SV_DispatchThreadID)
{
    UINT RayIndex = GroupPos.x * SSSR_INTERSECT_THREAD_COUNT + GroupIndex;
    UINT MaxRayCount = GetRayCount();
    BRANCH
    if (RayIndex >= MaxRayCount)
        return;

    UINT PackRayData = GetPackRayData(RayIndex);
    UINT2 SamplePos;
    bool bCopyHorizontal, bCopyVertical, bCopyDiagonal;
    GetUnpackRayData(PackRayData, SamplePos, bCopyHorizontal, bCopyVertical, bCopyDiagonal);

    const float2 ScreenUV = (float2(SamplePos) + 0.5f) * g_TargetSize.zw;
    const float SceneRawDepth = GetSceneDepth(SamplePos, 0);
    const float Roughness = GetRoughness(SamplePos);
    float3 NormalWS = LoadSSSRNormalWS(SamplePos);
    float3 NormalVS = TransformNormalFromWSToVS(NormalWS, viewMatrix).xyz;

    float3 PositionVS = ComputeViewSpacePosition(ScreenUV, SceneRawDepth, projMatrix_I);
    float3 ViewDirVS = normalize(PositionVS);

    const float3 RayOriginScreenspaceUV = float3(ScreenUV, SceneRawDepth);
    float3 ReflectDirVS = SampleReflectionVector(SamplePos, ViewDirVS, NormalVS, Roughness);
    float3 ReflectDirUV = SSSR_ProjectDirection(PositionVS, ReflectDirVS, RayOriginScreenspaceUV, projMatrix);

    bool bHit = false;
    float3 HitPosUV = RayMarch(RayOriginScreenspaceUV, ReflectDirUV, g_TargetSize.xy, bHit);

    float3 ReflectDirWS = mul(float4(ReflectDirVS, 0.0f), viewMatrix_I).xyz;

    float confidence = bHit ? SSSR_ValidateHit(HitPosUV, ScreenUV, ReflectDirWS, g_TargetSize.xy, g_Thickness) : 0.0f;
    float3 ReflectionRadiance = 0.0f;
    if (confidence > 0.0f)
    {
        // Camera Color is already pre-exposed in Opaque / Skybox.
        ReflectionRadiance = SampleTexture2D_LOD(g_CameraColorTexIndex, HitPosUV.xy, ClampLinearSampler, 0).rgb;
    }

    // Misses stay at alpha 0 so composite does not add environment IBL twice.
    float4 new_sample = float4(ReflectionRadiance, confidence);
    RWTexture2D<float4> IntersectionOutputTex = ResourceDescriptorHeap[g_IntersectionOutputTexIndex];
    IntersectionOutputTex[SamplePos] = new_sample;

    uint2 copy_target = SamplePos ^ 0b1;
    if (bCopyHorizontal)
    {
        uint2 copy_coords = uint2(copy_target.x, SamplePos.y);
        IntersectionOutputTex[copy_coords] = new_sample;
    }
    if (bCopyVertical)
    {
        uint2 copy_coords = uint2(SamplePos.x, copy_target.y);
        IntersectionOutputTex[copy_coords] = new_sample;
    }
    if (bCopyDiagonal)
    {
        IntersectionOutputTex[copy_target] = new_sample;
    }
}
