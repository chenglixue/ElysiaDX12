#pragma once
#include "Runtime/Engine/ECS/Transform.h"

namespace ElysiaRenderer
{
    using namespace ElysiaEngine;
    using namespace SimpleMath;

    class DX12Camera
    {
    public:
        Transform m_transform;

    public:
        DX12Camera() = default;
        DX12Camera(float nearZ, float farZ) noexcept;
        DX12Camera(const DX12Camera& rhs) = default;
        DX12Camera& operator=(const DX12Camera& rhs) = default;
        DX12Camera(DX12Camera&& rhs) = default;
        ~DX12Camera();

        Vector3 GetPosition() const noexcept;
        Vector4 GetPosition4() const noexcept;
        Quaternion GetRotation() const noexcept;
        float GetNearZ() const noexcept;
        float GetFarZ() const noexcept;
        Matrix GetViewMat() const noexcept;
        Matrix GetProjMat() const noexcept;
        Vector3 GetForwardDir() const noexcept;
        Vector3 GetUpDir() const noexcept;
        Vector3 GetRightDir() const noexcept;

        void SetPosition(const Vector3& cameraPos) noexcept;
        void SetRotation(const float x, const float y, const float z) noexcept;
        void SetNearZ(float nearZ) noexcept;
        void SetFarz(float farZ) noexcept;

        void Rotate(const Vector3& pitchYawRollOffset) noexcept;
        void Translate(const Vector3& translateOffset) noexcept;

        void LookAt(const Vector3& targetPos) noexcept;
        void UpdateViewMatrix() noexcept;
        virtual void UpdateProjMatrix() noexcept = 0;

        Quaternion CreateFromAxisAngle(const Vector3& axis, float angle);
        Quaternion Euler(float x, float y, float z);

    protected:
        Matrix m_viewMatrix = Matrix::Identity;
        Matrix m_projMatrix = Matrix::Identity;

        float m_nearZ = 0.01f;
        float m_farZ = 1000.f;
    };

    class PerspectiveCamera : public DX12Camera
    {
    public:
        PerspectiveCamera(float nearZ,
                          float farZ,
                          float aspectRatio,
                          float fovY);

        float GetFOVY() const noexcept
        {
            return m_fovy;
        }
        void Setfovy(float fovy) noexcept
        {
            m_fovy = fovy;
            UpdateProjMatrix();
        }

        float GetAspect() const noexcept
        {
            return m_aspectRatio;
        }
        void SetAspectRatio(float aspectRatio) noexcept
        {
            m_aspectRatio = aspectRatio;
            UpdateProjMatrix();
        }

        virtual void UpdateProjMatrix() noexcept override;

    private:
        float m_aspectRatio = 1.f;
        float m_fovx = 0.8f;
        float m_fovy = 0.8f;
    };

    class FirstPersonCamera : public PerspectiveCamera
    {
    public:
        using PerspectiveCamera::PerspectiveCamera;
        float GetCameraSpeed() const noexcept
        {
            return m_speed;
        }
        void SetCameraSpeed(float speed) noexcept
        {
            m_speed = speed;
        }

        void AddYawPitch(float yawDelta, float pitchDelta) noexcept;
        void Move(const Vector3& direction, float deltaTime) noexcept;
        void SyncFromTransform();

    private:
        float m_speed = 1.f;
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
    };
}