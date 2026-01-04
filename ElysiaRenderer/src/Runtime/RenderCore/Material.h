#pragma once
#include "Programs/Helper.h"

#include "Runtime/Core/ShaderUtility.h"
#include "Programs/Hash.h"
#include "MaterialParams.h"
#include "Runtime/Core/DX12RootSignature.h"

namespace std
{
    template <>
    struct std::hash<std::vector<std::wstring>>
    {
        using argument_type = std::vector<std::wstring>;
        using result_type = size_t;

        size_t operator()(argument_type const& v) const
        {
            return xxh::GetHash(v);
        }
    };

    template <>
    struct equal_to<std::vector<std::wstring>>
    {
        using argument_type = std::vector<std::wstring>;
        using result_type = size_t;

        bool operator()(argument_type const& a, argument_type const& b) const
        {
            return memcmp(&a, &b, sizeof(argument_type)) == 0;
        }
    };
}

namespace ElysiaCore
{
    class DX12Device;
    class DX12Shader;
    struct PipelineStateObject;
    class UploadRingBuffer;
}

namespace ElysiaRenderer
{
    using namespace ElysiaCore;

    struct PassData
    {
        UINT PassIndex;
        std::string Name;
        std::unique_ptr<DX12Shader> pShader = nullptr;
        std::unique_ptr<DX12RootSignature> pRootSignature = nullptr;
        PipelineResourceMapping resourceMapping;
        D3D12_RASTERIZER_DESC RasterizerDesc;
        D3D12_BLEND_DESC BlendDesc;
        D3D12_DEPTH_STENCIL_DESC DepthStencilDesc;
        ShaderVariantData* pCurrVariantData = nullptr;
        PipelineStateObject* pPipelineStateObject = nullptr;

        PipelineResourceLayout* GetMeshResourceLayout()
        {
            assert(pCurrVariantData);
            return pCurrVariantData->pMeshResourceLayout.get();
        }

        UINT3 GetKernelThreadGroupSizes()
        {
            auto size = pCurrVariantData->MergedReflectionData.ThreadGroupSize;
            if (!size.IsValid())
            {
                ShowErrorMessageBox(L"cur shader pass not support compute shader");
            }
            return {size.X, size.Y, size.Z};
        }
    };

    class Material
    {
    public:
        Material();
        Material(DX12Device* pDevice, std::vector<ShaderPass>& shaderPasses);
        ~Material();

        void Init(std::vector<ShaderPass>& shaderPasses);
        PassData& GetPassData(UINT passIndex) noexcept;
        const UINT FindPassIndex(const std::string& name) const noexcept;

        MaterialParameterBlock& GetParameterBlock()
        {
            return m_parameterBlock;
        }

        void SetInt(size_t nameHash, int v, size_t passID = 0);
        void SetUInt(size_t nameHash, unsigned int v, size_t passID = 0);
        void SetBool(size_t nameHash, bool v, size_t passID = 0);
        void SetFloat(size_t nameHash, float v, size_t passID = 0);
        void SetFloat2(size_t nameHash, const Vector2& v, size_t passID = 0);
        void SetFloat3(size_t nameHash, const Vector3& v, size_t passID = 0);
        void SetFloat4(size_t nameHash, const Vector4& v, size_t passID = 0);
        void SetMatrix(size_t nameHash, const Matrix& m, size_t passID = 0);
        void SetMatrix(size_t nameHash, const math::Matrix4& m, size_t passID = 0);
        void SetFloatArray(size_t nameHash, const std::vector<float>& values, size_t passID = 0);
        void SetIntArray(size_t nameHash, const std::vector<int>& values, size_t passID = 0);
        void SetUINTArray(size_t nameHash, const std::vector<UINT>& values, size_t passID = 0);
        void SetVector2Array(size_t nameHash, const std::vector<Vector2>& values,
                             size_t passID = 0);
        void SetVector3Array(size_t nameHash, const std::vector<Vector3>& values,
                             size_t passID = 0);
        void SetVector4Array(size_t nameHash, const std::vector<Vector4>& values,
                             size_t passID = 0);
        void SetMatrixArray(size_t nameHash, const std::vector<Matrix>& values, size_t passID = 0);

    private:
        std::mutex m_setDataMutex;
        std::vector<PassData> m_passDatas;
        DX12Device* m_pDevice = nullptr;
        MaterialParameterBlock m_parameterBlock; // 所有材质参数
    };
}