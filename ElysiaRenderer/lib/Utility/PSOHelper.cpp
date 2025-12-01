#include "stdafx.h"

#include "PSOHelper.h"

namespace ElysiaHelper
{
    static const UINT64 NumBlendStates = UINT64(BlendState::NumValues);
    static const UINT64 NumRasterizerStates = UINT64(RasterizerState::NumValues);
    static const UINT64 NumDepthStates = UINT64(DepthState::NumValues);

    static D3D12_BLEND_DESC BlendStateDescs[NumBlendStates] = { };
    static D3D12_RASTERIZER_DESC RasterizerStateDescs[NumRasterizerStates] = { };
    static D3D12_DEPTH_STENCIL_DESC DepthStateDescs[NumBlendStates] = { };

    void InitPSOHelpers()
    {
        InitBlend();
        InitRasterizer();
        InitDepthStencil();
    }

    void InitBlend()
    {
        {
            D3D12_BLEND_DESC& blendDesc = BlendStateDescs[UINT64(BlendState::Disabled)];
            blendDesc.RenderTarget[0].BlendEnable = FALSE;
            blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
            blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        }

        {
            D3D12_BLEND_DESC& blendDesc = BlendStateDescs[UINT64(BlendState::Additive)];
            blendDesc.RenderTarget[0].BlendEnable = true;
            blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        }

        {
            D3D12_BLEND_DESC& blendDesc = BlendStateDescs[UINT64(BlendState::AlphaBlend)];
            blendDesc.RenderTarget[0].BlendEnable = true;
            blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        }

        {
            D3D12_BLEND_DESC& blendDesc = BlendStateDescs[UINT64(BlendState::PreMultiplied)];
            blendDesc.RenderTarget[0].BlendEnable = false;
            blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        }

        {
            D3D12_BLEND_DESC& blendDesc = BlendStateDescs[UINT64(BlendState::NoColorWrites)];
            blendDesc.RenderTarget[0].BlendEnable = false;
            blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
            blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        }

        {
            D3D12_BLEND_DESC& blendDesc = BlendStateDescs[UINT64(BlendState::PreMultipliedRGB)];
            blendDesc.RenderTarget[0].BlendEnable = true;
            blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC1_COLOR;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
            blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        }
    }

    void InitRasterizer()
    {
        {
            D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[UINT64(RasterizerState::NoCull)];
            rastDesc.CullMode = D3D12_CULL_MODE_NONE;
            rastDesc.DepthClipEnable = true;
            rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
            rastDesc.MultisampleEnable = true;
        }

        {
            D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[UINT64(RasterizerState::FrontFaceCull)];
            rastDesc.CullMode = D3D12_CULL_MODE_FRONT;
            rastDesc.DepthClipEnable = true;
            rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
            rastDesc.MultisampleEnable = true;
        }

        {
            D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[UINT64(RasterizerState::BackFaceCull)];
            rastDesc.CullMode = D3D12_CULL_MODE_BACK;
            rastDesc.DepthClipEnable = TRUE;
            rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
            rastDesc.MultisampleEnable = true;
        }

        {
            D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[UINT64(RasterizerState::BackFaceCullNoZClip)];
            rastDesc.CullMode = D3D12_CULL_MODE_BACK;
            rastDesc.DepthClipEnable = false;
            rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
            rastDesc.MultisampleEnable = true;
        }

        {
            D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[UINT64(RasterizerState::NoCullNoMS)];
            rastDesc.CullMode = D3D12_CULL_MODE_NONE;
            rastDesc.DepthClipEnable = true;
            rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
            rastDesc.MultisampleEnable = false;
        }

        {
            D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[UINT64(RasterizerState::Wireframe)];
            rastDesc.CullMode = D3D12_CULL_MODE_NONE;
            rastDesc.DepthClipEnable = true;
            rastDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
            rastDesc.MultisampleEnable = true;
        }
    }

    void InitDepthStencil()
    {
        {
            D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[UINT64(DepthState::Disabled)];
            dsDesc.DepthEnable = false;
            dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        }

        {
            D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[UINT64(DepthState::Enabled)];
            dsDesc.DepthEnable = true;
            dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        }

        {
            D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[UINT64(DepthState::Reversed)];
            dsDesc.DepthEnable = true;
            dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        }

        {
            D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[UINT64(DepthState::WritesEnabled)];
            dsDesc.DepthEnable = true;
            dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        }

        {
            D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[UINT64(DepthState::ReversedWritesEnabled)];
            dsDesc.DepthEnable = true;
            dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        }
    }

    D3D12_BLEND_DESC GetBlendState(BlendState blendState)
    {
        assert(UINT64(blendState) < ArraySize_(BlendStateDescs));

        return BlendStateDescs[UINT64(blendState)];
    }
    D3D12_BLEND_DESC GetBlendState(std::wstring blendStateName)
    {
        BlendState blendState;
        switch (blendStateName)
        {
            case L"Disabled":
            {
                blendState = BlendState::Disabled;
                break;
            }
            case L"Additive":
            {
                blendState = BlendState::Additive;
                break;
            }
            case L"AlphaBlend":
            {
                blendState = BlendState::AlphaBlend;
                break;
            }
            case L"PreMultiplied":
            {
                blendState = BlendState::PreMultiplied;
                break;
            }
            case L"NoColorWrites":
            {
                blendState = BlendState::NoColorWrites;
                break;
            }
            case L"PreMultipliedRGB":
            {
                blendState = BlendState::PreMultipliedRGB;
                break;
            }
        }

        return GetBlendState(blendState);
    }

    D3D12_RASTERIZER_DESC GetRasterizerState(RasterizerState rasterizerState)
    {
        assert(UINT64(rasterizerState) < ArraySize_(RasterizerStateDescs));

        return RasterizerStateDescs[UINT64(rasterizerState)];
    }
    D3D12_RASTERIZER_DESC GetRasterizerState(std::wstring rasterizerStateName)
    {
        RasterizerState rasterizerState;
        switch (rasterizerStateName)
        {
            case L"NoCull":
            {
                rasterizerState = RasterizerState::NoCull;
                break;
            }
            case L"BackFaceCull":
            {
                rasterizerState = RasterizerState::BackFaceCull;
                break;
            }
            case L"BackFaceCullNoZClip":
            {
                rasterizerState = RasterizerState::BackFaceCullNoZClip;
                break;
            }
            case L"FrontFaceCull":
            {
                rasterizerState = RasterizerState::FrontFaceCull;
                break;
            }
            case L"NoCullNoMS":
            {
                rasterizerState = RasterizerState::NoCullNoMS;
                break;
            }
            case L"Wireframe":
            {
                rasterizerState = RasterizerState::Wireframe;
                break;
            }
        }

        return GetRasterizerState(rasterizerState);
    }

    D3D12_DEPTH_STENCIL_DESC GetDepthState(DepthState depthState)
    {
        assert(UINT64(depthState) < ArraySize_(DepthStateDescs));

        return DepthStateDescs[UINT64(depthState)];
    }
    D3D12_DEPTH_STENCIL_DESC GetDepthState(std::wstring depthStateName)
    {
        DepthState depthState;
        switch (depthStateName)
        {
            case L"Disabled":
            {
                depthState = DepthState::Disabled;
                break;
            }
            case L"Enabled":
            {
                depthState = DepthState::Enabled;
                break;
            }
            case L"Reversed":
            {
                depthState = DepthState::Reversed;
                break;
            }
            case L"WritesEnabled":
            {
                depthState = DepthState::WritesEnabled;
                break;
            }
            case L"ReversedWritesEnabled":
            {
                depthState = DepthState::ReversedWritesEnabled;
                break;
            }
        }

        return GetDepthState(depthState);
    }
}