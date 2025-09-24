#if defined(EDITOR)
    //#include <private\ShadingCommon.hlsl>

    //#include <private\Light.hlsl>
    //#include <private\LightCommon.hlsl>
      #include <private\SharedCommon.hlsli>
#else
    //#include "../private\ShadingCommon.hlsl"

    //#include "../private\Light.hlsl"
    //#include "../private\LightCommon.hlsl"
    #include "../private\SharedCommon.hlsli"
#endif

ConstantBuffer<PassConstant> PassConstantBuffer : register(b0, perPassSpace);
ConstantBuffer<ObjectConstant> ObjectConstantBuffer : register(b0, perObjectSpace);

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float4 positionVS : VIEW_POSITION;
    float4 positionWS : WORLD_POSITION;
    float3 normalWS : NORMAL;
    float3 tangentWS : TANGENT;
    float3 bitTangentWS : BITTANGENT;
    float2 uv : TEXCOORD;
    float3 color : COLOR;
};

struct PSOutput
{
    float4 target0 : SV_TARGET0;
};

PSInput VS(uint vertexId : SV_VertexID)
{
    PSInput o = (PSInput) 0;
    
    ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[ObjectConstantBuffer.vertexBufferIndex];
    
    DX12Vertex vertex = vertexBuffer.Load<DX12Vertex>(vertexId * sizeof(DX12Vertex));

    o.positionWS = mul(ObjectConstantBuffer.worldMatrix, float4(vertex.position, 1.f));
    o.positionVS = mul(PassConstantBuffer.viewMatrix, o.positionWS);
    o.positionCS = mul(PassConstantBuffer.projMatrix, o.positionVS);
    
    //bool hasTangent = true;
    //if (hasTangent)
    //{
    //    float3 N = normalize(mul(i.normalOS, (float3x3) M_World));
    //    float3 T = mul(i.tangentOS, (float3x3) M_World);
        
    //    o.tangentWS = normalize(T - dot(N , T) * N);
    //    o.bitTangentWS = (cross(o.tangentWS, N));
    //    o.normalWS = N;
    //}
    //else
    //{
    //    o.normalWS = normalize(mul(i.normalOS, (float3x3) M_World));
    //}
    
    o.uv = vertex.uv;
    o.color = vertex.color;
    
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
    inputParam.ScreenUV = i.positionCS.xy / PassConstantBuffer.screenSize.xy;
    inputParam.TangentWS = i.tangentWS;
    inputParam.BitTangentWS = i.bitTangentWS;
    inputParam.NormalWS = i.normalWS;
    inputParam.ScreenVector = GetScreenVectorWS(PassConstantBuffer.cameraPosWS.xyz, i.positionWS.xyz);
    
    //LightData mainLight = GetMainLight(PassConstantBuffer.lights[0]);
    
    //MaterialData materialData = GetMaterialData(inputParam);
    
    //o.target0 = GetDynamicLighting(inputParam, materialData, mainLight);
    o.target0.rgb = 0;
    
    //float4 shadowPos = mul(float4(inputParam.PositionWS, 1.f), M_Shadow);
    //shadowPos /= shadowPos.w;
    //shadowPos.xy = shadowPos.xy * float2(0.5f, -0.5f) + 0.5f;
    
    ////float shadowDepth = g_ShadowTex.Sample(g_Sampler_ClampU_ClampV_Linear, shadowPos.xy);
    //float shadowDepth = g_ShadowTex.Sample(g_Sampler_ClampU_ClampV_Linear, inputParam.ScreenUV);
    //float currDepth = step(shadowPos.z, shadowDepth);
    //o.target0 = currDepth;
    
    return o;
}