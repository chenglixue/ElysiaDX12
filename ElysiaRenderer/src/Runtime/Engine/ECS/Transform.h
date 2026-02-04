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

        Vector3 GetEulerDegrees() const
        {
            // 将四元数转换为欧拉角（弧度），然后转为角度
            // SimpleMath/DirectXMath 默认顺序通常是 Y-P-R
            Matrix R = Matrix::CreateFromQuaternion(rotation);

            float pitch, yaw, roll;
            // 简单的从矩阵提取欧拉角的逻辑
            pitch = asinf(-R._32);
            if (cosf(pitch) > 0.0001f)
            {
                yaw = atan2f(R._31, R._33);
                roll = atan2f(R._12, R._22);
            }
            else
            {
                yaw = 0.0f;
                roll = atan2f(-R._21, R._11);
            }

            return Vector3(XMConvertToDegrees(pitch),
                           XMConvertToDegrees(yaw),
                           XMConvertToDegrees(roll));
        }
    };
}