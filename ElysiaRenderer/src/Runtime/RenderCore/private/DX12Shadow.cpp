#include "stdafx.h"
#include "../public/DX12Shadow.h"

#include "Programs/public/Helper.h"
#include "../public/DX12Light.h"
#include "Runtime/Core/public/DX12TextureBuffer.h"
#include "Runtime/RenderCore/public/DX12Camera.h"

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    struct AxisWindow
    {
        float Min;
        float Max;
        float Texel;
    };

    float NextPowerOfTwo(float value)
    {
        const float safe = std::max(value, 1.e-3f);
        const float exponent = std::ceil(std::log2(safe) - 1.e-5f);
        return std::exp2(exponent);
    }

    float SnapDown(float value, float step)
    {
        return std::floor(value / step) * step;
    }

    bool Overlaps(float minA, float maxA, float minB, float maxB)
    {
        return minA <= maxB && maxA >= minB;
    }

    Vector3 StableLightUp(const Vector3& lightDir, const Vector3& previousUp)
    {
        Vector3 up = previousUp - lightDir * previousUp.Dot(lightDir);
        if (up.LengthSquared() < 1.e-6f)
            up = std::abs(lightDir.y) < 0.99f ? Vector3::UnitY : Vector3::UnitX;
        up.Normalize();
        return up;
    }

    AxisWindow FitAxis(float receiverMin, float receiverMax, UINT resolution)
    {
        float quantized = NextPowerOfTwo(receiverMax - receiverMin);
        for (int step = 0; step < 4; ++step)
        {
            const float texel = quantized / float(resolution);
            const float minimum = SnapDown(receiverMin, texel);
            if (minimum + quantized + 1.e-4f >= receiverMax)
                return {minimum, minimum + quantized, texel};
            quantized = NextPowerOfTwo(quantized + texel);
        }

        const float texel = quantized / float(resolution);
        const float minimum = SnapDown(receiverMin, texel);
        return {minimum, minimum + quantized, texel};
    }

    void BuildLightSpaceAabb(const Vector3* points,
                             size_t count,
                             const Matrix& lightView,
                             float& minX,
                             float& minY,
                             float& minZ,
                             float& maxX,
                             float& maxY,
                             float& maxZ)
    {
        minX = minY = minZ = FLT_MAX;
        maxX = maxY = maxZ = -FLT_MAX;
        for (size_t i = 0; i < count; ++i)
        {
            const Vector3 p = Vector3::Transform(points[i], lightView);
            minX = std::min(minX, p.x);
            minY = std::min(minY, p.y);
            minZ = std::min(minZ, p.z);
            maxX = std::max(maxX, p.x);
            maxY = std::max(maxY, p.y);
            maxZ = std::max(maxZ, p.z);
        }
    }

    DX12Shadow::DX12Shadow(DX12TextureResource* buffer)
        : m_buffer(buffer)
    {
        m_width = static_cast<UINT>(m_buffer->GetResourceDesc().Width);
        m_height = m_buffer->GetResourceDesc().Height;
        m_format = m_buffer->GetResourceDesc().Format;

        ZeroMemory(&m_viewPort, sizeof(D3D12_VIEWPORT));
        ZeroMemory(&m_scissorRect, sizeof(D3D12_RECT));

        CreateViewport();
        CreateScissorRect();
    }

    DX12Shadow::~DX12Shadow()
    {

    }

    const UINT& DX12Shadow::GetWidth() const noexcept
    {
        return m_width;
    }
    const UINT& DX12Shadow::GetHeight() const noexcept
    {
        return m_height;
    }
    const DX12TextureResource* DX12Shadow::GetShadowRT() const noexcept
    {
        return m_buffer;
    }
    const D3D12_VIEWPORT& DX12Shadow::GetViewport() const noexcept
    {
        return m_viewPort;
    }
    const D3D12_RECT& DX12Shadow::GetScissorRect() const noexcept
    {
        return m_scissorRect;
    }
    const float& DX12Shadow::GetNearZ() const noexcept
    {
        return m_nearZ;
    }
    const float& DX12Shadow::GetFarZ() const noexcept
    {
        return m_farZ;
    }
    const Matrix& DX12Shadow::GetView() const noexcept
    {
        return m_shadowViewMatrix;
    }
    const Matrix& DX12Shadow::GetProj() const noexcept
    {
        return m_shadowProjMatrix;
    }
    const Matrix& DX12Shadow::GetShadowMat() const noexcept
    {
        return m_shadowMatrix;
    }

    void DX12Shadow::CreateViewport()
    {
        m_viewPort.Width = static_cast<float>(m_width);
        m_viewPort.Height = static_cast<float>(m_height);
        m_viewPort.TopLeftX = 0;
        m_viewPort.TopLeftY = 0;
        m_viewPort.MinDepth = 0;
        m_viewPort.MaxDepth = 1;
    }
    void DX12Shadow::CreateScissorRect()
    {
        m_scissorRect.left = 0;
        m_scissorRect.right = m_width;
        m_scissorRect.bottom = m_height;
        m_scissorRect.top = 0;
    }

    void DX12Shadow::InitBoundSphere(float radius, Vector3 center)
    {
        m_shadowBound.Center = center;
        m_shadowBound.Radius = radius;
    }

    void DX12Shadow::UpdateShadowTransform(DX12Light* light,
                                           const DX12Camera& camera,
                                           float shadowDist,
                                           const BoundingBox* casterBounds,
                                           size_t casterCount)
    {
        // Build light view mat
        Vector3 lightDir = light->GetLightDir();
        lightDir.Normalize();
        m_lightUp = StableLightUp(lightDir, m_lightUp);

        const Matrix lightViewMat = Matrix::CreateLookAt(-lightDir, Vector3::Zero, m_lightUp);

        // Get Camera Frustum Corners
        Vector3 cameraCorners[8];
        camera.GetFrustum().GetCorners(cameraCorners);

        const float cameraNear = MathHelper::Max(camera.GetNearZ(), 1e-3f);
        const float cameraFar = MathHelper::Max(camera.GetFarZ(), cameraNear + 1e-3f);
        const float receiverFar = MathHelper::Clamp(shadowDist, cameraNear + 1e-3f, cameraFar);
        const float farScale = (receiverFar - cameraNear) / (cameraFar - cameraNear);

        // Get Receiver Frustum Corners
        Vector3 receiverCorners[8];
        for (UINT i = 0; i < 4; ++i)
        {
            const Vector3 nearCorner(cameraCorners[i]);
            const Vector3 farCorner(cameraCorners[i + 4]);

            receiverCorners[i] = nearCorner;
            receiverCorners[i + 4] = nearCorner + farScale * (farCorner - nearCorner);
        }

        // Transofrm Receiver Frustum Corners from world to light space
        float receiverMinX, receiverMinY, receiverMinZ;
        float receiverMaxX, receiverMaxY, receiverMaxZ;
        BuildLightSpaceAabb(receiverCorners,
                            8,
                            lightViewMat,
                            receiverMinX,
                            receiverMinY,
                            receiverMinZ,
                            receiverMaxX,
                            receiverMaxY,
                            receiverMaxZ);

        // 按最远距离估一个 texel，给 PCF 留边，避免接收者贴在阴影图边缘。
        const UINT resolution = std::max(m_width, 1u);
        const float edgePad = (2.f * receiverFar) / float(resolution);
        const AxisWindow axisX = FitAxis(receiverMinX - edgePad, receiverMaxX + edgePad, resolution);
        const AxisWindow axisY = FitAxis(receiverMinY - edgePad, receiverMaxY + edgePad, resolution);

        // 平行光下，只有光源空间 XY 和接收区重叠的物体才会把影子投进画面。
        float casterNear = receiverMinZ;
        for (size_t casterIndex = 0; casterIndex < casterCount; ++casterIndex)
        {
            XMFLOAT3 rawCorners[8];
            casterBounds[casterIndex].GetCorners(rawCorners);

            Vector3 corners[8];
            for (int i = 0; i < 8; ++i)
                corners[i] = Vector3(rawCorners[i]);

            float minX, minY, minZ, maxX, maxY, maxZ;
            BuildLightSpaceAabb(corners, 8, lightViewMat, minX, minY, minZ, maxX, maxY, maxZ);
            if (!Overlaps(minX, maxX, axisX.Min, axisX.Max) ||
                !Overlaps(minY, maxY, axisY.Min, axisY.Max))
                continue;

            casterNear = std::min(casterNear, minZ);
        }
        casterNear = std::max(casterNear, receiverMinZ - receiverFar);

        const float zStep = std::max(axisX.Texel, axisY.Texel);
        m_nearZ = SnapDown(casterNear, zStep);
        m_farZ = SnapDown(receiverMaxZ, zStep) + zStep;
        if (m_farZ - m_nearZ < zStep)
            m_farZ = m_nearZ + zStep;

        const Matrix lightProjMat = Matrix::CreateOrthographicOffCenter(axisX.Min,
                                                                        axisX.Max,
                                                                        axisY.Min,
                                                                        axisY.Max,
                                                                        m_nearZ,
                                                                        m_farZ);

        // m_lightPos = -lightDir * m_shadowBound.Radius;
        // Vector3 boundPosWS = m_shadowBound.Center;
        // Vector3 lightUp = Vector3::Up;
        // Matrix lightViewMat = Matrix::CreateLookAt(m_lightPos, boundPosWS, lightUp);
        // //XMMatrixLookAtLH(m_lightPos, boundPosWS, lightUp);
        //
        // Vector3 boundPosLS = Vector3::Zero;
        // Vector3::Transform(boundPosWS, lightViewMat, boundPosLS);
        //
        // float l = boundPosLS.x - m_shadowBound.Radius;
        // float r = boundPosLS.x + m_shadowBound.Radius;
        // float t = boundPosLS.y + m_shadowBound.Radius;
        // float b = boundPosLS.y - m_shadowBound.Radius;
        // float n = boundPosLS.z - m_shadowBound.Radius;
        // float f = boundPosLS.z + m_shadowBound.Radius;
        // m_nearZ = n;
        // m_farZ = f;
        // auto LS2ProjMat = Matrix::CreateOrthographicOffCenter(l, r, b, t, n, f);

        m_shadowMatrix = lightViewMat * lightProjMat;
        m_shadowViewMatrix = lightViewMat;
        m_shadowProjMatrix = lightProjMat;
        m_lightPos = -lightDir * MathHelper::Max(receiverFar, 1.f);
    }
}