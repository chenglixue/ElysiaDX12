#include "stdafx.h"
#include "MaterialParams.h"

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    bool MaterialParameterBlock::ParamValue::Equals(const ParamValue& other, Type type, float tolerance) const
    {
        if (IsArrayType(type))
        {
            if (arrayData.size() != other.arrayData.size())
                return false;
            if (arrayData.empty())
                return true;

            if (type == Type::FloatArray || type == Type::Float2Array ||
                type == Type::Float3Array || type == Type::Float4Array || type == Type::MatrixArray)
            {
                // 只有浮点数组需要逐元素 tolerance 比较
                const float* a = reinterpret_cast<const float*>(arrayData.data());
                const float* b = reinterpret_cast<const float*>(other.arrayData.data());
                size_t count = arrayData.size() / sizeof(float);
                for (size_t i = 0; i < count; ++i)
                {
                    if (std::abs(a[i] - b[i]) > tolerance)
                        return false;
                }
                return true;
            }
            else
            {
                // 整数数组直接内存比较
                return std::memcmp(arrayData.data(), other.arrayData.data(), arrayData.size()) == 0;
            }
        }

        const float* a = reinterpret_cast<const float*>(data.data());
        const float* b = reinterpret_cast<const float*>(other.data.data());

        switch (type)
        {
        case Type::FLOAT:
            return FloatEqual(a[0], b[0], tolerance);

        case Type::FLOAT2:
            return FloatEqual(a[0], b[0], tolerance) &&
                   FloatEqual(a[1], b[1], tolerance);

        case Type::FLOAT3:
            return FloatEqual(a[0], b[0], tolerance) &&
                   FloatEqual(a[1], b[1], tolerance) &&
                   FloatEqual(a[2], b[2], tolerance);

        case Type::FLOAT4:
            return FloatEqual(a[0], b[0], tolerance) &&
                   FloatEqual(a[1], b[1], tolerance) &&
                   FloatEqual(a[2], b[2], tolerance) &&
                   FloatEqual(a[3], b[3], tolerance);

        case Type::MATRIX4X4:
            for (int i = 0; i < 16; ++i)
            {
                if (!FloatEqual(a[i], b[i], tolerance))
                    return false;
            }
            return true;

        case Type::INT:
        case Type::UInt:
        case Type::BOOL:
            return std::memcmp(data.data(), other.data.data(), sizeof(UINT)) == 0;

        }

        return false;
    }

    void MaterialParameterBlock::MarkAsDirty()
    {
        m_isDirty = true;

        // 如果有脏状态回调，调用它
        if (m_dirtyCallback)
        {
            m_dirtyCallback();
        }
    }

    void MaterialParameterBlock::ClearDirty()
    {
        m_isDirty = false;
    }

    bool MaterialParameterBlock::IsDirty() const
    {
        return m_isDirty;
    }

    void MaterialParameterBlock::SetDirtyCallback(std::function<void()> callback)
    {
        m_dirtyCallback = callback;
    }

    template <typename T>
    void MaterialParameterBlock::SetOrAdd(size_t nameHash, Type type, const T& value)
    {
        auto it = std::find_if(m_params.begin(), m_params.end(),
                               [nameHash](const MaterialParam& p)
                               {
                                   return p.nameHash == nameHash;
                               });

        if (it != m_params.end())
        {
            // 检查类型是否匹配
            if (it->type != type)
            {
                it->type = type;
                it->value = ParamValue(); // 重置值
                SetValue(it->value, value);
                MarkAsDirty(); // 类型改变，标记为脏
            }
            else
            {
                // 创建临时值用于比较
                ParamValue tempValue;
                SetValue(tempValue, value);

                // 检查值是否相等（使用容忍度）
                const float tolerance = 1e-6f; // 浮点数比较容忍度
                if (!it->value.Equals(tempValue, type, tolerance))
                {
                    // 值不同，更新并标记为脏
                    SetValue(it->value, value);
                    MarkAsDirty();
                }
            }
        }
        else
        {
            // 添加新参数
            MaterialParam param;
            param.nameHash = nameHash;
            param.type = type;
            SetValue(param.value, value);
            m_params.emplace_back(std::move(param));
            MarkAsDirty(); // 新参数，标记为脏
        }
    }

    template <typename T>
    void MaterialParameterBlock::SetOrAddArray(size_t nameHash, Type type, const std::vector<T>& values)
    {
        auto it = std::find_if(m_params.begin(), m_params.end(),
                               [nameHash](const MaterialParam& p)
                               {
                                   return p.nameHash == nameHash;
                               });

        if (it != m_params.end())
        {
            if (it->type != type)
            {
                it->type = type;
                it->value = ParamValue(); // 重置值
                SetValue(it->value, values);
                MarkAsDirty(); // 类型改变，标记为脏
            }
            else
            {
                // 创建临时值用于比较
                ParamValue tempValue;
                SetValue(tempValue, values);

                // 检查值是否相等（使用容忍度）
                const float tolerance = 1e-6f; // 浮点数比较容忍度
                if (!it->value.Equals(tempValue, type, tolerance))
                {
                    // 值不同，更新并标记为脏
                    SetValue(it->value, values);
                    MarkAsDirty();
                }
            }
        }
        else
        {
            MaterialParam param;
            param.nameHash = nameHash;
            param.type = type;
            SetValue(param.value, values);
            m_params.emplace_back(std::move(param));
            MarkAsDirty(); // 新参数，标记为脏
        }

    }

    template <typename T>
    void MaterialParameterBlock::SetOrAddWithPassID(size_t nameHash, Type type, const T& value, size_t passID)
    {
        auto scopedHash = MixPassAndName(passID, nameHash);
        SetOrAdd(scopedHash, type, value);
    }

    template <typename T>
    void MaterialParameterBlock::SetOrAddArrayWithPassID(size_t nameHash, Type type, const std::vector<T>& values,
                                                         size_t passID)
    {
        auto scopedHash = MixPassAndName(passID, nameHash);
        SetOrAddArray(scopedHash, type, values);
    }

    size_t MaterialParameterBlock::MixPassAndName(size_t passID, size_t nameHash) const
    {
        constexpr size_t sizeBits = sizeof(size_t) * 8;

        if constexpr (sizeBits == 64)
        {
            return (passID << 32) | (nameHash & 0xFFFFFFFF);
        }
        else
        {
            // 32位系统下的简单混合 (减少冲突)
            // 使用黄金比例乘数进行混合
            return passID * 0x9E3779B9U ^ nameHash;
        }
    }


    void MaterialParameterBlock::SetFloat(size_t nameHash, float v, size_t passID)
    {
        SetOrAddWithPassID(nameHash, Type::FLOAT, v, passID);
    }

    void MaterialParameterBlock::SetInt(size_t nameHash, int v, size_t passID)
    {
        SetOrAddWithPassID(nameHash, Type::INT, v, passID);
    }

    void MaterialParameterBlock::SetUInt(size_t nameHash, unsigned int v, size_t passID)
    {
        SetOrAddWithPassID(nameHash, Type::UInt, v, passID);
    }

    void MaterialParameterBlock::SetBool(size_t nameHash, bool v, size_t passID)
    {
        uint32_t uv = v ? 1 : 0;
        SetOrAddWithPassID(nameHash, Type::BOOL, uv, passID);
    }

    void MaterialParameterBlock::SetFloat2(size_t nameHash, const Vector2& v, size_t passID)
    {
        SetOrAddWithPassID(nameHash, Type::FLOAT2, *reinterpret_cast<const Vector2*>(&v), passID);
    }

    void MaterialParameterBlock::SetFloat3(size_t nameHash, const Vector3& v, size_t passID)
    {
        SetOrAddWithPassID(nameHash, Type::FLOAT3, *reinterpret_cast<const Vector3*>(&v), passID);
    }

    void MaterialParameterBlock::SetFloat4(size_t nameHash, const Vector4& v, size_t passID)
    {
        SetOrAddWithPassID(nameHash, Type::FLOAT4, *reinterpret_cast<const Vector4*>(&v), passID);
    }

    void MaterialParameterBlock::SetMatrix(size_t nameHash, const Matrix& m, size_t passID)
    {
        SetOrAddWithPassID(nameHash, Type::MATRIX4X4, m, passID);
    }

    void MaterialParameterBlock::SetMatrix(size_t nameHash, const math::Matrix4& m, size_t passID)
    {
        SetOrAddWithPassID(nameHash, Type::MATRIX4X4, transpose(m), passID);
    }

    void MaterialParameterBlock::SetFloatArray(size_t nameHash, const std::vector<float>& values, size_t passID)
    {
        SetOrAddArrayWithPassID(nameHash, Type::FloatArray, values, passID);
    }

    void MaterialParameterBlock::SetIntArray(size_t nameHash, const std::vector<int>& values, size_t passID)
    {
        SetOrAddArrayWithPassID(nameHash, Type::IntArray, std::move(values), passID);
    }

    void MaterialParameterBlock::SetUINTArray(size_t nameHash, const std::vector<uint32_t>& values, size_t passID)
    {
        SetOrAddArrayWithPassID(nameHash, Type::UIntArray, std::move(values), passID);
    }

    void MaterialParameterBlock::SetVector2Array(size_t nameHash, const std::vector<Vector2>& values, size_t passID)
    {
        std::vector<uint8_t> byteData(values.size() * sizeof(float));
        std::memcpy(byteData.data(), values.data(), byteData.size());
        SetOrAddArrayWithPassID(nameHash, Type::Float2Array, values, passID);
    }

    void MaterialParameterBlock::SetVector3Array(size_t nameHash, const std::vector<Vector3>& values, size_t passID)
    {
        std::vector<uint8_t> byteData(values.size() * sizeof(float));
        std::memcpy(byteData.data(), values.data(), byteData.size());
        SetOrAddArrayWithPassID(nameHash, Type::Float3Array, values, passID);
    }

    void MaterialParameterBlock::SetVector4Array(size_t nameHash, const std::vector<Vector4>& values, size_t passID)
    {
        std::vector<uint8_t> byteData(values.size() * sizeof(float));
        std::memcpy(byteData.data(), values.data(), byteData.size());
        SetOrAddArrayWithPassID(nameHash, Type::Float4Array, values, passID);
    }

    void MaterialParameterBlock::SetMatrixArray(size_t nameHash, const std::vector<Matrix>& values, size_t passID)
    {
        std::vector<uint8_t> byteData(values.size() * sizeof(float));
        std::memcpy(byteData.data(), values.data(), byteData.size());
        SetOrAddArrayWithPassID(nameHash, Type::MatrixArray, values, passID);
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, float v)
    {
        memset(dst.data.data(), 0, 64);
        *reinterpret_cast<float*>(dst.data.data()) = v;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, int v)
    {
        memset(dst.data.data(), 0, 64);
        memcpy(dst.data.data(), &v, sizeof(int));
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, UINT v)
    {
        memset(dst.data.data(), 0, 64);
        memcpy(dst.data.data(), &v, sizeof(UINT));
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const Vector2& v)
    {
        memset(dst.data.data(), 0, 64);
        *reinterpret_cast<Vector2*>(dst.data.data()) = v;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const Vector3& v)
    {
        memset(dst.data.data(), 0, 64);
        *reinterpret_cast<Vector3*>(dst.data.data()) = v;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const Vector4& v)
    {
        memset(dst.data.data(), 0, 64);
        *reinterpret_cast<Vector4*>(dst.data.data()) = v;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const Matrix& m)
    {
        memset(dst.data.data(), 0, 64);
        std::memcpy(dst.data.data(), &m, sizeof(Matrix));
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const math::Matrix4& m)
    {
        memset(dst.data.data(), 0, 64);
        std::memcpy(dst.data.data(), &m, sizeof(math::Matrix4));
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<float>& floatArray)
    {
        dst.arrayData.resize(floatArray.size() * sizeof(float));
        std::memcpy(dst.arrayData.data(), floatArray.data(), dst.arrayData.size());
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<int>& intArray)
    {
        dst.arrayData.resize(intArray.size() * sizeof(int));
        std::memcpy(dst.arrayData.data(), intArray.data(), dst.arrayData.size());
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<UINT>& UINTArray)
    {
        dst.arrayData.resize(UINTArray.size() * sizeof(UINT));
        std::memcpy(dst.arrayData.data(), UINTArray.data(), dst.arrayData.size());
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<Vector2>& Vector2Array)
    {
        size_t byteSize = Vector2Array.size() * sizeof(Vector2);
        dst.arrayData.resize(byteSize);
        std::memcpy(dst.arrayData.data(), Vector2Array.data(), byteSize);
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<Vector3>& Vector3Array)
    {
        size_t byteSize = Vector3Array.size() * sizeof(Vector3);
        dst.arrayData.resize(byteSize);
        std::memcpy(dst.arrayData.data(), Vector3Array.data(), byteSize);
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<Vector4>& Vector4Array)
    {
        size_t byteSize = Vector4Array.size() * sizeof(Vector4);
        dst.arrayData.resize(byteSize);
        std::memcpy(dst.arrayData.data(), Vector4Array.data(), byteSize);
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<Matrix>& MatrixArray)
    {
        dst.arrayData.resize(MatrixArray.size() * sizeof(Matrix));
        std::memcpy(dst.arrayData.data(), MatrixArray.data(), dst.arrayData.size());
    }

    template void MaterialParameterBlock::SetOrAdd<float>(size_t, Type, const float&);
    template void MaterialParameterBlock::SetOrAdd<int>(size_t, Type, const int&);
    template void MaterialParameterBlock::SetOrAdd<unsigned int>(size_t, Type, const unsigned int&);
    template void MaterialParameterBlock::SetOrAdd<Vector2>(size_t, Type, const Vector2&);
    template void MaterialParameterBlock::SetOrAdd<Vector3>(size_t, Type, const Vector3&);
    template void MaterialParameterBlock::SetOrAdd<Vector4>(size_t, Type, const Vector4&);
    template void MaterialParameterBlock::SetOrAdd<Matrix>(size_t, Type, const Matrix&);
    template void MaterialParameterBlock::SetOrAddArray<float>(size_t nameHash, Type type,
                                                               const std::vector<float>& values);
    template void MaterialParameterBlock::SetOrAddArray<
        int>(size_t nameHash, Type type, const std::vector<int>& values);
    template void MaterialParameterBlock::SetOrAddArray<unsigned int>(size_t nameHash, Type type,
                                                                      const std::vector<unsigned int>& values);
    template void MaterialParameterBlock::SetOrAddArray<Vector2>(size_t nameHash, Type type,
                                                                 const std::vector<Vector2>& values);
    template void MaterialParameterBlock::SetOrAddArray<Vector3>(size_t nameHash, Type type,
                                                                 const std::vector<Vector3>& values);
    template void MaterialParameterBlock::SetOrAddArray<Vector4>(size_t nameHash, Type type,
                                                                 const std::vector<Vector4>& values);
    template void MaterialParameterBlock::SetOrAddArray<Matrix>(size_t nameHash, Type type,
                                                                const std::vector<Matrix>& values);
    template void MaterialParameterBlock::SetOrAddWithPassID<float>(size_t, Type, const float&, size_t);
    template void MaterialParameterBlock::SetOrAddWithPassID<int>(size_t, Type, const int&, size_t);
    template void MaterialParameterBlock::SetOrAddWithPassID<unsigned int>(size_t, Type, const unsigned int&, size_t);
    template void MaterialParameterBlock::SetOrAddWithPassID<Vector2>(size_t, Type, const Vector2&, size_t);
    template void MaterialParameterBlock::SetOrAddWithPassID<Vector3>(size_t, Type, const Vector3&, size_t);
    template void MaterialParameterBlock::SetOrAddWithPassID<Vector4>(size_t, Type, const Vector4&, size_t);
    template void MaterialParameterBlock::SetOrAddWithPassID<Matrix>(size_t, Type, const Matrix&, size_t);
    template void MaterialParameterBlock::SetOrAddArrayWithPassID<float>(
        size_t nameHash, Type type, const std::vector<float>& values, size_t);
    template void MaterialParameterBlock::SetOrAddArrayWithPassID<int>(size_t nameHash, Type type,
                                                                       const std::vector<int>& values, size_t);
    template void MaterialParameterBlock::SetOrAddArrayWithPassID<unsigned int>(
        size_t nameHash, Type type, const std::vector<unsigned int>& values, size_t);
    template void MaterialParameterBlock::SetOrAddArrayWithPassID<Vector2>(
        size_t nameHash, Type type, const std::vector<Vector2>& values, size_t);
    template void MaterialParameterBlock::SetOrAddArrayWithPassID<Vector3>(
        size_t nameHash, Type type, const std::vector<Vector3>& values, size_t);
    template void MaterialParameterBlock::SetOrAddArrayWithPassID<Vector4>(
        size_t nameHash, Type type, const std::vector<Vector4>& values, size_t);
    template void MaterialParameterBlock::SetOrAddArrayWithPassID<Matrix>(
        size_t nameHash, Type type, const std::vector<Matrix>& values, size_t);

    const MaterialParameterBlock::MaterialParam* MaterialParameterBlock::FindParam(size_t nameHash, size_t passID) const
    {
        // 第一步：查找 Scoped (特定 Pass 的覆盖)
        // 只有当传入的 passID 不是全局 ID (0) 时才执行
        if (passID != 0)
        {
            auto scopedHash = MixPassAndName(passID, nameHash);
            auto it = std::find_if(m_params.begin(), m_params.end(),
                                   [scopedHash](const MaterialParam& p)
                                   {
                                       return p.nameHash == scopedHash;
                                   });

            if (it != m_params.end())
                return &(*it);
        }

        // 第二步：回退查找 Global (全局 ID = 0)
        auto globalHash = MixPassAndName(0, nameHash);
        auto itGlobal = std::find_if(m_params.begin(), m_params.end(),
                                     [globalHash](const MaterialParam& p)
                                     {
                                         return p.nameHash == globalHash;
                                     });

        return (itGlobal != m_params.end()) ? &(*itGlobal) : nullptr;
    }

    MaterialParameterBlock::MaterialParam* MaterialParameterBlock::FindParam(size_t nameHash, size_t passID)
    {
        // 第一步：查找 Scoped (特定 Pass 的覆盖)
        // 只有当传入的 passID 不是全局 ID (0) 时才执行
        if (passID != 0)
        {
            auto scopedHash = MixPassAndName(passID, nameHash);
            auto it = std::find_if(m_params.begin(), m_params.end(),
                                   [scopedHash](const MaterialParam& p)
                                   {
                                       return p.nameHash == scopedHash;
                                   });

            if (it != m_params.end())
                return &(*it);
        }

        // 第二步：回退查找 Global (全局 ID = 0)
        auto globalHash = MixPassAndName(0, nameHash);
        auto itGlobal = std::find_if(m_params.begin(), m_params.end(),
                                     [globalHash](const MaterialParam& p)
                                     {
                                         return p.nameHash == globalHash;
                                     });

        return (itGlobal != m_params.end()) ? &(*itGlobal) : nullptr;
    }

    void MaterialParameterBlock::RemoveParam(size_t nameHash)
    {
        m_params.erase(
            std::remove_if(m_params.begin(), m_params.end(),
                           [nameHash](const MaterialParam& p)
                           {
                               return p.nameHash == nameHash;
                           }),
            m_params.end());
    }

    void MaterialParameterBlock::MergeFrom(const MaterialParameterBlock& other)
    {
        for (const auto& srcParam : other.GetParams())
        {
            auto* dstParam = FindParam(srcParam.nameHash);
            if (!dstParam)
            {
                m_params.push_back(srcParam);
            }
            else
            {
                if (dstParam->type != srcParam.type)
                {
                    dstParam->type = srcParam.type;
                    dstParam->value = srcParam.value;
                }
                else
                {
                    if (!dstParam->value.Equals(srcParam.value, srcParam.type))
                    {
                        dstParam->value = srcParam.value;
                    }
                }
            }
        }
    }


}