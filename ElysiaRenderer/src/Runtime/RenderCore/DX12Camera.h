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
        DX12Camera(const Transform& transform,
                   float aspectRatio,
                   float fovY,
                   float nearZ,
                   float farZ) noexcept;
        DX12Camera(const DX12Camera& rhs) = default;
        DX12Camera& operator=(const DX12Camera& rhs) = default;
        DX12Camera(DX12Camera&& rhs) = default;
        ~DX12Camera();

        float GetCameraSpeed() const noexcept;
        Vector3 GetPosition() const noexcept;
        Vector4 GetPosition4() const noexcept;
        Quaternion GetRotation() const noexcept;
        float GetNearZ() const noexcept;
        float GetFarZ() const noexcept;
        float GetFOVY() const noexcept;
        float GetAspect() const noexcept;
        Matrix GetViewMat() const noexcept;
        Matrix GetProjMat() const noexcept;
        Vector3 GetForwardDir() const noexcept;
        Vector3 GetUpDir() const noexcept;
        Vector3 GetRightDir() const noexcept;

        void SetCameraSpeed(float speed) noexcept;
        void SetPosition(const Vector3& cameraPos) noexcept;
        void SetRotation(const Quaternion& rotation) noexcept;
        void SetAspectRatio(float aspectRatio) noexcept;
        void SetNearZ(float nearZ) noexcept;
        void SetFarz(float farZ) noexcept;
        void Setfovy(float FOV) noexcept;

        void Rotate(const Vector3& pitchYawRollOffset) noexcept;
        void Translate(const Vector3& translateOffset) noexcept;

        void LookAt(const Vector3& targetPos) noexcept;
        void UpdateViewMatrix() noexcept;
        // after roate, three vec in camera would not Non-orthogonal normalized vector 
        void UpdateProjMatrix() noexcept;

    protected:
        Matrix m_viewMatrix = Matrix::Identity;
        Matrix m_projMatrix = Matrix::Identity;

        float m_speed = 1.f;
        float m_aspectRatio = 1.f;
        float m_fovy = 0.8f;
        float m_fovx = 0.8f;
        float m_nearZ = 0.01f;
        float m_farZ = 300.f;
    };
}