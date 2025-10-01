#if defined(EDITOR)
    #include <private\ShadingCommon.hlsl>

    #include <private\Light.hlsl>
    #include <private\LightCommon.hlsl>
      #include <private\SharedCommon.hlsli>
#else
    #include "../private\ShadingCommon.hlsl"

    #include "../private\Light.hlsl"
    #include "../private\LightCommon.hlsl"
    #include "../private\SharedCommon.hlsli"
#endif

struct VSInput
{
    float3 positionOS : POSITION;
    float3 uv : TEXCOORD0;
    float3 normalOS : NORMAL;
    float3 tangentOS : TANGENT;
};

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float4 positionVS : VIEW_POSITION;
    float4 positionWS : WORLD_POSITION;
    float3 normalWS : NORMAL;
    float3 tangentWS : TANGENT;
    float3 bitTangentWS : BITTANGENT;
    float2 uv : TEXCOORD;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(VSInput i)
{
    PSInput o = (PSInput) 0;
    
    //ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[ObjectConstantBuffer.vertexBufferIndex];
    
    //DX12Vertex vertex = vertexBuffer.Load<DX12Vertex>(vertexId * sizeof(DX12Vertex));

    o.positionWS = mul(worldMatrix, float4(i.positionOS, 1.f));
    o.positionVS = mul(viewMatrix, o.positionWS);
    o.positionCS = mul(projMatrix, o.positionVS);
    
    bool hasTangent = true;
    if (hasTangent)
    {
        float3 N = normalize(mul((float3x3) worldMatrix, i.normalOS));
        float3 T = mul((float3x3) worldMatrix, i.tangentOS);
        
        o.tangentWS = normalize(T - dot(N, T) * N);
        o.bitTangentWS = (cross(o.tangentWS, N));
        o.normalWS = N;
    }
    else
    {
        o.normalWS = normalize(mul((float3x3) worldMatrix, i.normalOS));
    }
    
    o.uv = i.uv;
    
    return o;
}

PSOutput PS(PSInput i)
{
    PSOutput o = (PSOutput) 0;
    
    FInputParams inputParam = (FInputParams) 0;
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
    
    MaterialData materialData = GetMaterialData(inputParam);
    
    o.target0 = GetDynamicLighting(inputParam, materialData, mainLightData);
    o.target0.rgb = materialData.WorldNormal;
    
    //float4 shadowPos = mul(float4(inputParam.PositionWS, 1.f), M_Shadow);
    //shadowPos /= shadowPos.w;
    //shadowPos.xy = shadowPos.xy * float2(0.5f, -0.5f) + 0.5f;
    
    ////float shadowDepth = g_ShadowTex.Sample(g_Sampler_ClampU_ClampV_Linear, shadowPos.xy);
    //float shadowDepth = g_ShadowTex.Sample(g_Sampler_ClampU_ClampV_Linear, inputParam.ScreenUV);
    //float currDepth = step(shadowPos.z, shadowDepth);
    //o.target0 = currDepth;
    
    return o;
}