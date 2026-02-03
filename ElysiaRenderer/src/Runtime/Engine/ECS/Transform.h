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

        Matrix GetWorldMatrix() const
        {
            auto localMat = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rotation) *
                            Matrix::CreateTranslation(position);
            if (m_pParent)
            {
                return localMat * m_pParent->GetWorldMatrix();
            }
            else
            {
                return localMat;
            }

        }

        void LookAt(Vector3 target)
        {
            Matrix lookAt = Matrix::CreateLookAt(position, target, Vector3::Up);

            Vector3 trashScale;
            Vector3 trashTranslation;
            lookAt.Invert().Decompose(trashScale, rotation, trashTranslation);
        }

        void RotateAxis(Vector3 axis, float angle)
        {
            rotation *= Quaternion::CreateFromAxisAngle(axis, angle);
        }

        bool operator==(const Transform& other) const
        {
            if (this->position == other.position && this->rotation == other.rotation && this->scale
                == other.scale)
            {
                return true;
            }
        }
    };
}