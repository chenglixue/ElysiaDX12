#ifndef SSAOCommon_h
#define SSAOCommon_h

#include "ShadingCommon.hlsl"

// 0: not similar .. 1:very similar
float ComputeDepthSimilarity(float DepthA, float DepthB, float TweakScale)
{
    return saturate(1 - abs(DepthA - DepthB) * TweakScale);
}

// 0: not similar .. 1:very similar
float ComputeDepthSimilarity(float DepthA, float DepthB)
{
    // 计算线性深度差
    float d1 = LinearEyeDepth(DepthA, g_ZBufferParams);
    float d2 = LinearEyeDepth(DepthB, g_ZBufferParams);

    // 深度感知阈值：如果差距超过基础深度的 5%，权重迅速衰减
    float diff = abs(d1 - d2);
    float threshold = d2 * 0.05f;

    return exp(-diff / (threshold + 1e-5));
}


#endif