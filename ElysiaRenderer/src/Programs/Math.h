#pragma once
#include "stdafx.h"

namespace ElysiaHelper
{
    class MathHelper
    {
    public:
        // Returns random float in [0, 1).
        static float RandF()
        {
            return (float)(rand()) / (float)RAND_MAX;
        }

        // Returns random float in [a, b).
        static float RandF(float a, float b)
        {
            return a + RandF() * (b - a);
        }

        static int Rand(int a, int b)
        {
            return a + rand() % ((b - a) + 1);
        }

        template <typename T>
        static T Min(const T& a, const T& b)
        {
            return a < b ? a : b;
        }

        template <typename T>
        static T Max(const T& a, const T& b)
        {
            return a > b ? a : b;
        }

        template <typename T>
        static T Lerp(const T& a, const T& b, float t)
        {
            return a + (b - a) * t;
        }

        template <typename T>
        static T Clamp(const T& x, const T& low, const T& high)
        {
            return x < low ? low : (x > high ? high : x);
        }

        static uint32_t CeilDivide(uint32_t value, uint32_t divisor)
        {
            return (value + divisor - 1) / divisor;
        }

        static Quaternion Euler(float x, float y, float z)
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
    };

    struct UINT2
    {
        UINT2() = default;
        UINT2(UINT x, UINT y)
        {
            this->x = x;
            this->y = y;
        }
        UINT2(const Vector2& rhs)
        {
            this->x = static_cast<UINT>(rhs.x);
            this->y = static_cast<UINT>(rhs.y);
        }
        uint32_t x = 0;
        uint32_t y = 0;
    };

    struct UINT3
    {
        UINT3(UINT x, UINT y, UINT z)
        {
            this->x = x;
            this->y = y;
            this->z = z;
        }
        UINT3(const Vector3& rhs)
        {
            this->x = static_cast<UINT>(rhs.x);
            this->y = static_cast<UINT>(rhs.y);
            this->z = static_cast<UINT>(rhs.z);
        }
        UINT3& operator=(const UINT3& rhs) = default;
        UINT3(const UINT3& rhs) = default;
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t z = 0;
    };

    static size_t Absdiff(size_t a, size_t b)
    {
        return a > b ? a - b : b - a;
    }

    inline uint32_t CeilDivide(uint32_t value, uint32_t divisor)
    {
        return (value + divisor - 1) / divisor;
    }


}