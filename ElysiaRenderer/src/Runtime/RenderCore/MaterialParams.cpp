#include "stdafx.h"
#include "MaterialParams.h"

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    bool MaterialParameterBlock::ParamValue::operator==(const ParamValue& other) const
    {
        return memcmp(data.data(), other.data.data(), sizeof(float) * 16) == 0 &&
               rowCount == other.rowCount && colCount == other.colCount;
    }

    bool MaterialParameterBlock::ParamValue::Equals(const ParamValue& other, Type type, float tolerance) const
    {
        auto arrayEqual = [](const std::vector<float>& arrayData, const ParamValue& other, size_t step, float tolerance)
        {
            auto o = true;
            for (size_t i = 0; i < arrayData.size(); i += step)
            {
                for (int j = 0; j < i + step; ++j)
                {
                    o &= FloatEqual(arrayData[j], other.data[j], tolerance);
                    if (!o)
                        return false;
                }
            }

            return o;
        };
        switch (type)
        {
        case Type::FLOAT:
            return FloatEqual(data[0], other.data[0], tolerance);

        case Type::INT:
            return *reinterpret_cast<const int*>(&data[0]) ==
                   *reinterpret_cast<const int*>(&other.data[0]);
        case Type::UInt:
            return FloatEqual(data[0], other.data[0], tolerance) &&
                   FloatEqual(data[1], other.data[1], tolerance);
        case Type::BOOL:
            return FloatEqual(data[0], other.data[0], tolerance);
        case Type::FLOAT2:
            return FloatEqual(data[0], other.data[0], tolerance) &&
                   FloatEqual(data[1], other.data[1], tolerance);

        case Type::FLOAT3:
            return FloatEqual(data[0], other.data[0], tolerance) &&
                   FloatEqual(data[1], other.data[1], tolerance) &&
                   FloatEqual(data[2], other.data[2], tolerance);

        case Type::FLOAT4:
            return FloatEqual(data[0], other.data[0], tolerance) &&
                   FloatEqual(data[1], other.data[1], tolerance) &&
                   FloatEqual(data[2], other.data[2], tolerance) &&
                   FloatEqual(data[3], other.data[3], tolerance);

        case Type::MATRIX4X4:
            for (int i = 0; i < 16; ++i)
            {
                if (!FloatEqual(data[i], other.data[i], tolerance))
                    return false;
            }
            return true;
        case Type::IntArray:
        {
            auto o = true;
            for (size_t i = 0; i < arrayData.size(); i++)
            {
                o &= *reinterpret_cast<const int*>(&data[i]) ==
                    *reinterpret_cast<const int*>(&other.data[i]);

                if (!o)
                    return false;
            }
            return true;
        }
        case Type::UIntArray:
        {
            auto o = true;
            for (size_t i = 0; i < arrayData.size(); i++)
            {
                o &= *reinterpret_cast<const UINT*>(&data[i]) ==
                    *reinterpret_cast<const UINT*>(&other.data[i]);

                if (!o)
                    return false;
            }
            return true;
        }
        case Type::FloatArray:
        {
            return arrayEqual(arrayData, other, 1, tolerance);
        }
        case Type::Float2Array:
        {
            return arrayEqual(arrayData, other, 2, tolerance);
        }
        case Type::Float3Array:
        {
            return arrayEqual(arrayData, other, 3, tolerance);
        }
        case Type::Float4Array:
        {
            return arrayEqual(arrayData, other, 4, tolerance);
        }
        case Type::MatrixArray:
        {
            return arrayEqual(arrayData, other, 16, tolerance);
        }
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
        float fv = static_cast<float>(v);
        SetOrAddWithPassID(nameHash, Type::INT, fv, passID);
    }

    void MaterialParameterBlock::SetUInt(size_t nameHash, unsigned int v, size_t passID)
    {
        float fv = static_cast<float>(v);
        SetOrAddWithPassID(nameHash, Type::UInt, fv, passID);
    }

    void MaterialParameterBlock::SetBool(size_t nameHash, bool v, size_t passID)
    {
        float fv = v ? 1.f : 0.f;
        SetOrAddWithPassID(nameHash, Type::BOOL, fv, passID);
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
        SetOrAddWithPassID(nameHash, Type::MATRIX4X4, m, passID);
    }

    void MaterialParameterBlock::SetFloatArray(size_t nameHash, const std::vector<float>& values, size_t passID)
    {
        SetOrAddArrayWithPassID(nameHash, Type::FloatArray, values, passID);
    }

    void MaterialParameterBlock::SetIntArray(size_t nameHash, const std::vector<int>& values, size_t passID)
    {
        std::vector<float> tempVec{};
        tempVec.reserve(values.size());
        for (size_t i = 0; i < values.size(); ++i)
        {
            tempVec.emplace_back(static_cast<float>(values[i]));
        }
        SetOrAddArrayWithPassID(nameHash, Type::IntArray, std::move(tempVec), passID);
    }

    void MaterialParameterBlock::SetUINTArray(size_t nameHash, const std::vector<uint32_t>& values, size_t passID)
    {
        std::vector<float> tempVec{};
        tempVec.reserve(values.size());
        for (size_t i = 0; i < values.size(); ++i)
        {
            tempVec.emplace_back(static_cast<float>(values[i]));
        }
        SetOrAddArrayWithPassID(nameHash, Type::UIntArray, std::move(tempVec), passID);
    }

    void MaterialParameterBlock::SetVector2Array(size_t nameHash, const std::vector<Vector2>& values, size_t passID)
    {
        SetOrAddArrayWithPassID(nameHash, Type::Float2Array, values, passID);
    }

    void MaterialParameterBlock::SetVector3Array(size_t nameHash, const std::vector<Vector3>& values, size_t passID)
    {
        SetOrAddArrayWithPassID(nameHash, Type::Float3Array, values, passID);
    }

    void MaterialParameterBlock::SetVector4Array(size_t nameHash, const std::vector<Vector4>& values, size_t passID)
    {
        SetOrAddArrayWithPassID(nameHash, Type::Float4Array, values, passID);
    }

    void MaterialParameterBlock::SetMatrixArray(size_t nameHash, const std::vector<Matrix>& values, size_t passID)
    {
        SetOrAddArrayWithPassID(nameHash, Type::MatrixArray, values, passID);
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, float v)
    {
        dst.data[0] = v;
        dst.rowCount = 1;
        dst.colCount = 1;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, int v)
    {
        *reinterpret_cast<int*>(&dst.data[0]) = v;
        dst.rowCount = 1;
        dst.colCount = 1;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, unsigned int v)
    {
        *reinterpret_cast<unsigned int*>(&dst.data[0]) = v;
        dst.rowCount = 1;
        dst.colCount = 1;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const Vector2& v)
    {
        dst.data[0] = v.x;
        dst.data[1] = v.y;
        dst.rowCount = 1;
        dst.colCount = 2;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const Vector3& v)
    {
        dst.data[0] = v.x;
        dst.data[1] = v.y;
        dst.data[2] = v.z;
        dst.rowCount = 1;
        dst.colCount = 3;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const Vector4& v)
    {
        dst.data[0] = v.x;
        dst.data[1] = v.y;
        dst.data[2] = v.z;
        dst.data[3] = v.w;
        dst.rowCount = 1;
        dst.colCount = 4;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const Matrix& m)
    {
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                dst.data[i * 4 + j] = m.m[i][j];
            }
        }
        dst.rowCount = 4;
        dst.colCount = 4;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const math::Matrix4& m)
    {
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                dst.data[i * 4 + j] = m[i][j];
            }
        }
        dst.rowCount = 4;
        dst.colCount = 4;
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<float>& floatArray)
    {
        dst.arrayData.reserve(floatArray.size());
        for (auto& value : floatArray)
        {
            dst.arrayData.emplace_back(value);
        }
        dst.rowCount = 1;
        dst.colCount = UINT(floatArray.size());
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<int>& intArray)
    {
        dst.arrayData.reserve(intArray.size());
        for (size_t i = 0; i < intArray.size(); i++)
        {
            dst.arrayData.emplace_back(intArray[i]);
        }
        dst.rowCount = 1;
        dst.colCount = UINT(intArray.size());
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<UINT>& UINTArray)
    {
        dst.arrayData.reserve(UINTArray.size());
        for (size_t i = 0; i < UINTArray.size(); i++)
        {
            *reinterpret_cast<UINT*>(&dst.arrayData[i]) = UINTArray[i];
        }
        dst.rowCount = 1;
        dst.colCount = UINT(UINTArray.size());
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<Vector2>& Vector2Array)
    {
        dst.arrayData.reserve(2 * Vector2Array.size());
        for (auto& value : Vector2Array)
        {
            dst.arrayData.emplace_back(value.x);
            dst.arrayData.emplace_back(value.y);
        }
        dst.rowCount = 1;
        dst.colCount = UINT(dst.arrayData.size());
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<Vector3>& Vector3Array)
    {
        dst.arrayData.reserve(3 * Vector3Array.size());
        for (auto& value : Vector3Array)
        {
            dst.arrayData.emplace_back(value.x);
            dst.arrayData.emplace_back(value.y);
            dst.arrayData.emplace_back(value.z);
        }
        dst.rowCount = 1;
        dst.colCount = UINT(dst.arrayData.size());
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<Vector4>& Vector4Array)
    {
        dst.arrayData.reserve(4 * Vector4Array.size());
        for (auto& value : Vector4Array)
        {
            dst.arrayData.emplace_back(value.x);
            dst.arrayData.emplace_back(value.y);
            dst.arrayData.emplace_back(value.z);
            dst.arrayData.emplace_back(value.w);
        }
        dst.rowCount = 1;
        dst.colCount = dst.arrayData.size();
    }

    void MaterialParameterBlock::SetValue(ParamValue& dst, const std::vector<Matrix>& MatrixArray)
    {
        dst.arrayData.reserve(16 * MatrixArray.size());
        for (auto& value : MatrixArray)
        {
            std::vector<float> tempMatrix = {};
            tempMatrix.reserve(16);
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    tempMatrix[i * 4 + j] = value.m[i][j];
                }
            }
            dst.arrayData.insert(dst.arrayData.begin(), tempMatrix.begin(), tempMatrix.end());
        }
        dst.rowCount = 4;
        dst.colCount = MatrixArray.size() * 4;
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
        auto scopedHash = MixPassAndName(passID, nameHash);
        auto it = std::find_if(m_params.begin(), m_params.end(),
                               [scopedHash](const MaterialParam& p)
                               {
                                   return p.nameHash == scopedHash;
                               });
        return (it != m_params.end()) ? &(*it) : nullptr;
    }

    MaterialParameterBlock::MaterialParam* MaterialParameterBlock::FindParam(size_t nameHash, size_t passID)
    {
        auto scopedHash = MixPassAndName(passID, nameHash);
        auto it = std::find_if(m_params.begin(), m_params.end(),
                               [scopedHash](const MaterialParam& p)
                               {
                                   return p.nameHash == scopedHash;
                               });

        return (it != m_params.end()) ? &(*it) : nullptr;
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