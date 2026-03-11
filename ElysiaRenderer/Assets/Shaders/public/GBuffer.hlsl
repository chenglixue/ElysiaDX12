#include <private\ShadingCommon.hlsl>
#include <private\Light.hlsl>
#include <private\LightCommon.hlsl>
#include <private\ShadowCommon.hlsl>
#include <public\GI\Irradiance.hlsl>

#pragma Vertex VS
#pragma Pixel PS

#pragma Rasterizer BackFaceCull
#pragma Blend Disabled
#pragma Depth WritesEnabled

cbuffer MaterialConstant : register(b0, perMaterialSpace)
{
    UINT meshDataBufferIndex;
    UINT meshDataIndex;
};

cbuffer PassConstant : register(b0, perPassSpace)
{
    Vector4 screenSize;

    Matrix viewMatrix;
    Matrix viewMatrix_I;
    Matrix projMatrix;
    Matrix projMatrix_I;
    Matrix viewProjMatrix;
    Matrix viewProjMatrix_I;

    Matrix pre_viewMatrix;
    Matrix pre_viewMatrix_I;
    Matrix pre_projMatrix;
    Matrix pre_projMatrix_I;
    Matrix pre_viewProjMatrix;
    Matrix pre_viewProjMatrix_I;

    float3 g_GridDimensions;
    float g_ProbeNormalBias;
    float g_ProbeViewBias;
    float g_DDGIEncodingGamma;
    float3 g_GridOrigin;
    float3 g_GridSpacing;
    UINT g_IrradianceTexIndex;
    UINT g_DistanceTexIndex;
    // UINT g_ProbeOffsetsIndex;
    UINT g_ProbeStatesIndex;
    UINT g_ProbeOffsetIndexTexIndex;
    UINT g_ProbeRelocationLUTBufferIndex;
    float4 g_IrradianceTexSize;
    float4 g_DistanceTexSize;
    float3 g_AmbientTint;
    float g_AmbientIntensity;
}

struct MeshData
{
    Matrix world_M;

    Vector4 baseColorUVTransform;
    Vector4 normalUVTransform;
    Vector4 emissiveUVTransform;
    Vector4 occlusionUVTransform;
    Vector4 metallicRoughnessUVTransform;

    float opacity;
    float cutoff;
    UINT baseColorTexIndex;
    UINT normalTexIndex;

    UINT metallicTexIndex;
    UINT roughnessTexIndex;
    UINT specularTexIndex;
    float metallicIntensity;

    Vector4 baseColorTint;

    float roughnessIntensity;
    float normalIntensity;
    UINT vertexOffset;
    UINT indexOffset;
};

struct VSInput
{
    float3 positionOS : POSITION;
    float2 uv : TEXCOORD0;
    float3 normalOS : NORMAL;
    float4 tangentOS : TANGENT;
};

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float4 positionVS : VIEW_POSITION;
    float4 positionWS : WORLD_POSITION;
    float3 normalWS : NORMALWS;
    float3 tangentWS : TANGENTWS;
    float3 bitTangentWS : BITANGENTWS;
    float2 uv : TEXCOORD0;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
    float4 target1 : SV_TARGET1;
    float4 target2 : SV_TARGET2;
    float4 target3 : SV_TARGET3;
    float4 target4 : SV_TARGET4;
    float4 target5 : SV_TARGET5;
};

PSInput VS(VSInput i)
{
    PSInput o = (PSInput)0;

    StructuredBuffer<MeshData> meshDataBuffer = ResourceDescriptorHeap[meshDataBufferIndex];
    Matrix worldMatrix = meshDataBuffer[meshDataIndex].world_M;
    o.positionWS = mul(float4(i.positionOS, 1.f), worldMatrix);
    o.positionVS = mul(o.positionWS, viewMatrix);
    o.positionCS = mul(o.positionVS, projMatrix);

    float3 N = normalize(mul((float3x3)worldMatrix, i.normalOS));
    float3 T = normalize(mul((float3x3)worldMatrix, i.tangentOS));
    o.tangentWS = normalize(T - dot(N, T) * N);
    o.bitTangentWS = cross(o.tangentWS, N) * i.tangentOS.w;
    o.normalWS = N;

    //float handedness = dot(o.bitTangentWS, cross(o.normalWS, o.tangentWS)) > 0.0f ? 1.0f : -1.0f;
    //o.bitTangentWS *= handedness;

    o.uv = i.uv;

    return o;
}

FEncodeGBufferData GetEncodeGBufferData(FInputParams inputParams, float3 toLight);

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput)0;

    FInputParams inputParam = (FInputParams)0;
    inputParam.PositionWS = i.positionWS;
    inputParam.PositionVS = i.positionVS;
    inputParam.PixelPos = i.positionCS.xy;
    inputParam.objectUV = i.uv;
    inputParam.ScreenUV = i.positionCS.xy / screenSize.xy;
    inputParam.TangentWS = i.tangentWS;
    inputParam.BitTangentWS = i.bitTangentWS;
    inputParam.NormalWS = i.normalWS;
    inputParam.ScreenVector = GetScreenVectorWS(cameraPosWS.xyz, i.positionWS.xyz);

    LightData mainLightData = GetMainLight(mainLight);

    FEncodeGBufferData encodeGBufferData = GetEncodeGBufferData(inputParam, mainLightData.toLight);

    o.target0 = float4(encodeGBufferData.BaseColor,
                       EncodeMaterialFlags(encodeGBufferData.ShadingModelID));
    o.target1 = float4(encodeGBufferData.Metallic,
                       encodeGBufferData.Specular,
                       encodeGBufferData.Roughness,
                       encodeGBufferData.AO);
    o.target2 = float4(EncodeNormal(encodeGBufferData.WorldTangent), encodeGBufferData.Anisotropy);
    o.target3 = float4(EncodeNormal(encodeGBufferData.WorldNormal),
                       encodeGBufferData.PerObjectData);
    o.target4 = float4(encodeGBufferData.IBL * encodeGBufferData.AO, encodeGBufferData.Opacity);
    o.target5 = float4(encodeGBufferData.Velocity, 0.f, 0.f);

    return o;
}

FEncodeGBufferData GetEncodeGBufferData(FInputParams inputParams, float3 toLight)
{
    FEncodeGBufferData o = (FEncodeGBufferData)0;

    float3x3 TBN = float3x3(inputParams.TangentWS, inputParams.BitTangentWS, inputParams.NormalWS);

    StructuredBuffer<MeshData> meshDataBuffer = ResourceDescriptorHeap[meshDataBufferIndex];
    MeshData currMeshData = meshDataBuffer[meshDataIndex];

    float4 baseColor = SampleTexture2D_LOD(currMeshData.baseColorTexIndex,
                                           inputParams.objectUV,
                                           WarpLinearSampler,
                                           0)
                       * float4(currMeshData.baseColorTint.xyz, currMeshData.opacity);
    clip(baseColor.a - currMeshData.cutoff);

    float4 normalTS = SampleTexture2D(currMeshData.normalTexIndex,
                                      inputParams.objectUV,
                                      WarpLinearSampler);

    float metallic = SampleTexture2D(currMeshData.metallicTexIndex,
                                     inputParams.objectUV,
                                     WarpLinearSampler);
    metallic = saturate(metallic * currMeshData.metallicIntensity);

    float roughness = SampleTexture2D(currMeshData.roughnessTexIndex,
                                      inputParams.objectUV,
                                      WarpLinearSampler);
    roughness = saturate(roughness * currMeshData.roughnessIntensity);

    o.BaseColor = baseColor.rgb;
    o.ShadingModelID = FLT_MAX;
    o.ShadingModelID = Shading_Model_ID_Default_Lit;
    o.Opacity = baseColor.a;

    o.AO = 1;
    o.Metallic = metallic;
    o.Roughness = roughness;
    o.Specular = 0.f;

    o.WorldNormal = GetNormal(normalTS.rgb, TBN, currMeshData.normalIntensity);
    o.WorldTangent = TBN._m00_m01_m02;
    o.PerObjectData = 0.f;
    o.PerComputedShadow = 1.f;

    float4 preClipPos = mul(float4(inputParams.PositionWS, 1.f), pre_viewProjMatrix);
    preClipPos /= preClipPos.w;
    float2 preScreenUV = preClipPos.xy * 0.5f * float2(1.f, -1.f) + 0.5f;

    o.Velocity = inputParams.ScreenUV - preScreenUV;

    o.Anisotropy = 0;
    o.DiffuseColor = o.BaseColor - o.BaseColor * o.Metallic;
    o.SpecularColor = ComputeF0(o.Specular, o.BaseColor, o.Metallic);

    float blendWeight = DDGIGetVolumeBlendWeight(inputParams.PositionWS,
                                                 g_GridOrigin,
                                                 g_GridSpacing,
                                                 0,
                                                 float4(0, 0, 0, 1));
    if (blendWeight > 0.f)
    {
        o.IBL = SampleDDGI(inputParams.PositionWS,
                           o.WorldNormal,
                           DDGIGetSurfaceBias(o.WorldNormal,
                                              inputParams.ScreenVector,
                                              g_ProbeNormalBias,
                                              g_ProbeViewBias),
                           g_GridOrigin,
                           g_GridSpacing,
                           g_GridDimensions,
                           g_DDGIEncodingGamma,
                           g_IrradianceTexSize,
                           g_IrradianceTexIndex,
                           g_DistanceTexSize,
                           g_DistanceTexIndex,
                           g_ProbeOffsetIndexTexIndex,
                           g_ProbeRelocationLUTBufferIndex,
                           g_ProbeStatesIndex,
                           WarpLinearSampler
                    ) * g_AmbientTint * g_AmbientIntensity;
        o.IBL = baseColor.rgb / PI * o.IBL;
    }

    return o;
}