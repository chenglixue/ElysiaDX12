#pragma once
#include "stdafx.h"
#include "Common.h"

namespace ElysiaHelper
{
    enum class BlendState : UINT64
    {
        Disabled = 0,
        Additive,
        AlphaBlend,
        PreMultiplied,
        NoColorWrites,
        PreMultipliedRGB,

        NumValues
    };

    enum class RasterizerState : UINT64
    {
        NoCull = 0,
        BackFaceCull,
        BackFaceCullNoZClip,
        FrontFaceCull,
        NoCullNoMS,
        Wireframe,

        NumValues
    };

    enum class DepthState : UINT64
    {
        Disabled = 0,
        Enabled,
        Reversed,
        WritesEnabled,
        ReversedWritesEnabled,

        NumValues
    };

    void InitPSOHelpers();

    /// <summary>
    /// Blend state initialization
    /// </summary>
    void InitBlend();

    /// <summary>
    /// Rasterizer state initialization
    /// </summary>
    void InitRasterizer();

    /// <summary>
    /// Depth state initialization
    /// </summary>
    void InitDepthStencil();

    D3D12_BLEND_DESC GetBlendState(BlendState blendState);
    D3D12_BLEND_DESC GetBlendState(std::wstring blendStateName);
    
    D3D12_RASTERIZER_DESC GetRasterizerState(RasterizerState rasterizerState);
    D3D12_RASTERIZER_DESC GetRasterizerState(std::wstring rasterizerStateName);
    
    D3D12_DEPTH_STENCIL_DESC GetDepthState(DepthState depthState);
    D3D12_DEPTH_STENCIL_DESC GetDepthState(std::wstring depthStateName);
}