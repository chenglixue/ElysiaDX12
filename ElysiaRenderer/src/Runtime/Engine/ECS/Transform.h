#pragma once

namespace ElysiaEngine
{
    using namespace DirectX::SimpleMath;

    struct Transform
    {
        Transform* m_pParent = nullptr;

        Vector3 position = Vector3::Zero;
        Quaternion rotation = Quaternion::Identity;
        Vector3 scale = Vector3::One;

        Matrix GetWorldMatrix() const;
        Vector3 GetLocalPosition() const noexcept
        {
            return position;
        }
        Vector3 GetPosition() const
        {
            return m_pParent ? m_pParent->GetPosition() + position : position;
        }

        void LookAt(Vector3 target);

        void RotateAxis(Vector3 axis, float angle);

        bool operator==(const Transform& other) const
        {
            if (this->position == other.position && this->rotation == other.rotation && this->scale
                == other.scale)
            {
                return true;
            }
        }

        Vector3 GetEulerDegrees() const;
    };
}