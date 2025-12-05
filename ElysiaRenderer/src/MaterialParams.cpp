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
		switch (type)
		{
			case Type::FLOAT:
				return FloatEqual(data[0], other.data[0], tolerance);

			case Type::INT:
				return *reinterpret_cast<const int*>(&data[0]) == 
					   *reinterpret_cast<const int*>(&other.data[0]);
			case Type::UINT:
				return FloatEqual(data[0], other.data[0], tolerance) &&
					   FloatEqual(data[1], other.data[1], tolerance);

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
	
	template<typename T>
	void MaterialParameterBlock::SetOrAdd(size_t nameHash, Type type, const T& value)
	{
		auto it = std::find_if(m_params.begin(), m_params.end(),
			[nameHash](const MaterialParam& p) { return p.nameHash == nameHash; });
    
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
	
	void MaterialParameterBlock::SetFloat(size_t nameHash, float v)
	{
		SetOrAdd(nameHash, Type::FLOAT, v);
	}
	void MaterialParameterBlock::SetInt(size_t nameHash, int v)
	{
		float fv = static_cast<float>(v);
		SetOrAdd(nameHash, Type::INT, fv);
	}
	void MaterialParameterBlock::SetUInt(size_t nameHash, unsigned int v)
	{
		float fv = static_cast<float>(v);
		SetOrAdd(nameHash, Type::UINT, fv);
	}
	void MaterialParameterBlock::SetFloat2(size_t nameHash, const Vector2& v)
	{
		SetOrAdd(nameHash, Type::FLOAT2, *reinterpret_cast<const Vector2*>(&v));
	}
	void MaterialParameterBlock::SetFloat3(size_t nameHash, const Vector3& v)
	{
		SetOrAdd(nameHash, Type::FLOAT3, *reinterpret_cast<const Vector3*>(&v));
	}
	void MaterialParameterBlock::SetFloat4(size_t nameHash, const Vector4& v)
	{
		SetOrAdd(nameHash, Type::FLOAT4, *reinterpret_cast<const Vector4*>(&v));
	}
	void MaterialParameterBlock::SetMatrix(size_t nameHash, const Matrix& m)
	{
		SetOrAdd(nameHash, Type::MATRIX4X4, m);
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

	template void MaterialParameterBlock::SetOrAdd<float>(size_t, Type, const float&);
	template void MaterialParameterBlock::SetOrAdd<int>(size_t, Type, const int&);
	template void MaterialParameterBlock::SetOrAdd<unsigned int>(size_t, Type, const unsigned int&);
	template void MaterialParameterBlock::SetOrAdd<Vector2>(size_t, Type, const Vector2&);
	template void MaterialParameterBlock::SetOrAdd<Vector3>(size_t, Type, const Vector3&);
	template void MaterialParameterBlock::SetOrAdd<Vector4>(size_t, Type, const Vector4&);
	template void MaterialParameterBlock::SetOrAdd<Matrix>(size_t, Type, const Matrix&);
	
	const MaterialParameterBlock::MaterialParam* MaterialParameterBlock::FindParam(size_t nameHash) const
	{
		auto it = std::find_if(m_params.begin(), m_params.end(),
			[nameHash](const MaterialParam& p) { return p.nameHash == nameHash; });
		return (it != m_params.end()) ? &(*it) : nullptr;
	}

	MaterialParameterBlock::MaterialParam* MaterialParameterBlock::FindParam(size_t nameHash)
	{
		auto it = std::find_if(m_params.begin(), m_params.end(),
			[nameHash](const MaterialParam& p) { return p.nameHash == nameHash; });
		return (it != m_params.end()) ? &(*it) : nullptr;
	}

	void MaterialParameterBlock::RemoveParam(size_t nameHash)
	{
		m_params.erase(
			std::remove_if(m_params.begin(), m_params.end(),
				[nameHash](const MaterialParam& p) { return p.nameHash == nameHash; }),
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