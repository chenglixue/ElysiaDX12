#include "stdafx.h"
#include "Transform.h"

namespace ElysiaEngine
{
    Matrix Transform::GetWorldMatrix() const
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

    void Transform::LookAt(Vector3 target)
    {
        Matrix lookAt = Matrix::CreateLookAt(position, target, Vector3::Up);

        Vector3 trashScale;
        Vector3 trashTranslation;
        lookAt.Invert().Decompose(trashScale, rotation, trashTranslation);
    }

    void Transform::RotateAxis(Vector3 axis, float angle)
    {
        rotation *= Quaternion::CreateFromAxisAngle(axis, angle);
    }

    Vector3 Transform::GetEulerDegrees() const
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
}