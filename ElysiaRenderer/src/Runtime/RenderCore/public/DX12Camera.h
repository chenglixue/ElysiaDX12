#pragma once
#include "Runtime/Engine/ECS/public/Transform.h"

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
        BoundingFrustum GetFrustum() const noexcept;

        void SetPosition(const Vector3& cameraPos) noexcept;
        void SetRotation(const float x, const float y, const float z) noexcept;
        void SetRotation(Quaternion rotation) noexcept;
        void SetNearZ(float nearZ) noexcept;
        void SetFarz(float farZ) noexcept;

        void Rotate(const Vector3& pitchYawRollOffset) noexcept;
        void Translate(const Vector3& translateOffset) noexcept;
        void LookAt(const Vector3& targetPos) noexcept;

        void UpdateViewMatrix() noexcept;
        virtual void UpdateProjMatrix() noexcept = 0;
        void UpdateFrustum();

        Quaternion CreateFromAxisAngle(const Vector3& axis, float angle);

    protected:
        Matrix m_viewMatrix = Matrix::Identity;
        Matrix m_projMatrix = Matrix::Identity;

        BoundingFrustum m_localFrustum;
        BoundingFrustum m_worldFrustum;

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
        FirstPersonCamera(float nearZ,
                          float farZ,
                          float aspectRatio,
                          float fovY);
        float GetCameraSpeed() const noexcept
        {
            return m_speed;
        }
        void SetCameraSpeed(float speed) noexcept;
        // UE: Speed += Speed * factor. factor +0.1 scroll up, -0.1 scroll down.
        void AdjustCameraSpeed(float relativeFactor) noexcept;

        void SetXRotation(float xRotation)
        {
            m_pitch = std::clamp(xRotation, -XM_PIDIV2, XM_PIDIV2);
            SetRotation(Quaternion(XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0)));
        }
        float GetXRotation() const
        {
            return m_pitch;
        };

        void SetYRotation(float yRotation)
        {
            m_yaw = DirectX::XMScalarModAngle(yRotation);
            SetRotation(Quaternion(XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0)));
        }
        float GetYRotation() const
        {
            return m_yaw;
        };

        void AddYawPitch(float yawDelta, float pitchDelta) noexcept;
        void Move(const Vector3& direction, float deltaTime) noexcept;
        void SyncFromTransform();

        // World-space look direction. Matches Move(): +Z in the camera's rotation.
        Vector3 GetWorldForwardDir() const noexcept;

        // UE FViewportCameraTransform::LookAt. F and Alt-orbit both use this pivot.
        void SetLookAtLocation(const Vector3& lookAt) noexcept;
        const Vector3& GetLookAtLocation() const noexcept { return m_lookAt; }
        bool HasLookAt() const noexcept { return m_bHasLookAt; }

        // UE FEditorViewportClient::FocusViewportOnBox (perspective branch).
        void FocusViewportOnBox(const BoundingBox& box) noexcept;
        // UE ToggleOrbitCamera(true) then InputAxisForOrbit rotation mode.
        void BeginOrbitCamera() noexcept;
        void OrbitCamera(float yawDelta, float pitchDelta) noexcept;

    private:
        // Matches UE FEditorViewportCameraSpeedSettings AbsoluteMin/Max.
        static constexpr float kMinCameraSpeed = 0.0001f;
        static constexpr float kMaxCameraSpeed = 10000.0f;
        // UE EditorViewportClient MinimumFocusRadius is 10 cm.
        static constexpr float kMinimumFocusRadius = 0.1f;
        // UE OrbitConstants::IntialLookAtDistance is 1024 cm.
        static constexpr float kInitialLookAtDistance = 10.0f;

        float m_speed = 2.f;
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;

        Vector3 m_lookAt = Vector3::Zero;
        bool m_bHasLookAt = false;
        float m_orbitDistance = kInitialLookAtDistance;
    };
}