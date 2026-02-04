#include "stdafx.h"
#include "DX12Camera.h"

namespace ElysiaRenderer
{
    //=================================================================================================
    // Base Camera
    //=================================================================================================
    DX12Camera::DX12Camera(float nearZ,
                           float farZ) noexcept
        : m_nearZ(nearZ),
          m_farZ(farZ)
    {
        m_viewMatrix = DirectX::XMMatrixIdentity();
        m_projMatrix = DirectX::XMMatrixIdentity();
        m_transform =
        {
            .position = Vector3::Zero,
            .rotation = Quaternion::Identity,
            .scale = Vector3::One
        };

    }
    DX12Camera::~DX12Camera()
    {

    }

    Vector3 DX12Camera::GetPosition() const noexcept
    {
        return m_transform.position;
    }

    Vector4 DX12Camera::GetPosition4() const noexcept
    {
        return Vector4(m_transform.position.x, m_transform.position.y, m_transform.position.z, 1);
    }

    Quaternion DX12Camera::GetRotation() const noexcept
    {
        return m_transform.rotation;
    }

    float DX12Camera::GetNearZ() const noexcept
    {
        return m_nearZ;
    }

    float DX12Camera::GetFarZ() const noexcept
    {
        return m_farZ;
    }

    Matrix DX12Camera::GetViewMat() const noexcept
    {
        return m_viewMatrix;
    }

    Matrix DX12Camera::GetProjMat() const noexcept
    {
        return m_projMatrix;
    }

    Vector3 DX12Camera::GetForwardDir() const noexcept
    {
        return m_viewMatrix.Forward();
    }

    Vector3 DX12Camera::GetUpDir() const noexcept
    {
        return m_viewMatrix.Up();
    }

    Vector3 DX12Camera::GetRightDir() const noexcept
    {
        return m_viewMatrix.Right();
    }

    void DX12Camera::SetPosition(const Vector3& cameraPos) noexcept
    {
        m_transform.position = cameraPos;
        UpdateViewMatrix();
    }

    void DX12Camera::SetRotation(const float x, const float y, const float z) noexcept
    {
        m_transform.rotation = Euler(0, 90, 0);

        UpdateViewMatrix();
    }

    void DX12Camera::SetNearZ(float nearZ) noexcept
    {
        m_nearZ = nearZ;
        UpdateProjMatrix();
    }

    void DX12Camera::SetFarz(float farZ) noexcept
    {
        m_farZ = farZ;
        UpdateProjMatrix();
    }

    void DX12Camera::LookAt(const Vector3& targetPos) noexcept
    {
        m_viewMatrix = Matrix::CreateLookAt(m_transform.position, targetPos, Vector3::Up);
        Matrix invView = m_viewMatrix.Invert();
        m_transform.rotation = Quaternion::CreateFromRotationMatrix(invView);
    }

    void DX12Camera::Rotate(const Vector3& pitchYawRollOffset) noexcept
    {
        Quaternion temp1 = Quaternion(-pitchYawRollOffset.x, 0.f, 0.f, 1.f);
        Quaternion temp2 = Quaternion(
            Quaternion::CreateFromAxisAngle(Vector3::Up, -pitchYawRollOffset.y));
        m_transform.rotation = temp2 * m_transform.rotation * temp1;

        m_transform.rotation.Normalize();
        UpdateViewMatrix();
    }

    void DX12Camera::Translate(const Vector3& translateOffset) noexcept
    {
        Vector3 right = m_viewMatrix.Right();
        Vector3 up = m_viewMatrix.Up();
        Vector3 forward = m_viewMatrix.Forward();

        m_transform.position += translateOffset.z * forward + translateOffset.x * right +
            translateOffset.y * up;
        UpdateViewMatrix();
    }

    void DX12Camera::UpdateViewMatrix() noexcept
    {
        Matrix R = Matrix::CreateFromQuaternion(m_transform.rotation).Invert();
        Matrix T = Matrix::CreateTranslation(-m_transform.position);
        m_viewMatrix = T * R;
    }

    Quaternion DX12Camera::CreateFromAxisAngle(const Vector3& axis, float angle)
    {
        // 1. 计算半角
        float halfAngle = angle * 0.5f;
        float s = std::sin(halfAngle);
        float c = std::cos(halfAngle);

        // 2. 根据公式计算分量
        // x, y, z 分别是轴分量乘以 sin(theta/2)
        // w 分量是 cos(theta/2)
        return Quaternion(
            axis.x * s,
            axis.y * s,
            axis.z * s,
            c
            );
    }
    Quaternion DX12Camera::Euler(float x, float y, float z)
    {
        // 1. 将角度转换为弧度
        // DirectXMath 提供了 XMConvertToRadians 宏
        float pitch = XMConvertToRadians(x); // 绕 X 轴
        float yaw = XMConvertToRadians(y);   // 绕 Y 轴
        float roll = XMConvertToRadians(z);  // 绕 Z 轴

        // 2. 使用 DirectXMath 的内置函数计算
        // 注意：DirectX 的顺序通常是 Yaw-Pitch-Roll (Y-X-Z)
        XMVECTOR qVec = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);

        // 3. 返回你的 Quaternion 类对象
        return Quaternion(qVec);
    }
    void GetPitchYaw(const Quaternion& q, float& pitch, float& yaw)
    {
        // 1. 计算 Pitch (绕 X 轴)
        // 我们需要将 sinPitch 限制在 [-1, 1] 之间，防止 asin 报错
        float sinPitch = 2.0f * (q.w * q.x - q.y * q.z);

        // 处理万向锁情况（Pitch 接近 +/- 90度）
        if (std::abs(sinPitch) >= 1.0f)
        {
            // 如果 sinPitch 溢出，直接取 +/- 90 度 (PI/2)
            pitch = std::copysign(1.570796f, sinPitch);
        }
        else
        {
            pitch = std::asin(sinPitch);
        }

        // 2. 计算 Yaw (绕 Y 轴)
        float sinYaw = 2.0f * (q.w * q.y + q.z * q.x);
        float cosYaw = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
        yaw = std::atan2(sinYaw, cosYaw);

        // 如果需要角度制，可以在此转换
        // pitch = XMConvertToDegrees(pitch);
        // yaw = XMConvertToDegrees(yaw);
    }

    //=================================================================================================
    // PerspectiveCamera
    //=================================================================================================
    PerspectiveCamera::PerspectiveCamera(float nearZ,
                                         float farZ,
                                         float aspectRatio,
                                         float fovY)
        : DX12Camera(nearZ, farZ),
          m_aspectRatio(aspectRatio),
          m_fovy(fovY)
    {
        assert(aspectRatio > 0.f);
        assert(fovY > 0.f && fovY <= XM_PI);

        UpdateViewMatrix();
        UpdateProjMatrix();
    }

    void PerspectiveCamera::UpdateProjMatrix() noexcept
    {
        m_projMatrix = Matrix::CreatePerspectiveFieldOfView(m_fovy, m_aspectRatio, m_nearZ, m_farZ);
    }

    //=================================================================================================
    // FirstPersonCamera
    //=================================================================================================
    void FirstPersonCamera::AddYawPitch(float yawDelta, float pitchDelta) noexcept
    {
        m_yaw += yawDelta;
        m_pitch += pitchDelta;

        m_pitch = std::clamp(m_pitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

        m_transform.rotation = Quaternion::CreateFromYawPitchRoll(m_yaw, m_pitch, 0.0f);

        UpdateViewMatrix();
    }
    void FirstPersonCamera::Move(const Vector3& direction, float deltaTime) noexcept
    {
        float s = m_speed * deltaTime;

        // 获取相机当前的本地坐标轴
        Matrix invView = m_viewMatrix.Invert();
        Vector3 right = invView.Right();
        Vector3 forward = invView.Forward();

        // direction.x = 左右, direction.z = 前后
        m_transform.position += right * direction.x * s;
        m_transform.position += forward * direction.z * s;

        UpdateViewMatrix();
    }
    void FirstPersonCamera::SyncFromTransform()
    {
        UpdateViewMatrix();
        GetPitchYaw(m_transform.rotation, m_pitch, m_yaw);
    }

}