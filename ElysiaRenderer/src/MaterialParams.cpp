#include "stdafx.h"
#include "MaterialParams.h"

namespace ElysiaRenderer
{
	static inline bool FloatEqual(float a, float b, float eps = 1e-6f)
	{
		return std::abs(a - b) <= eps;
	}
	
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
			case Type::UINT:
				return static_cast<int>(data[0]) == static_cast<int>(other.data[0]);

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
	
	template<typename T>
	void MaterialParameterBlock::SetOrAdd(size_t nameHash, Type type, const T& value)
	{
		auto it = std::find_if(m_params.begin(), m_params.end(),
			[nameHash](const MaterialParam& p) { return p.nameHash == nameHash; });

		if (it != m_params.end())
		{
			if (it->type != type)
			{
				it->type = type;
				it->value = {}; // reset
			}
			memcpy(it->value.data.data(), &value, sizeof(T));
			it->value.rowCount = (type == Type::MATRIX4X4) ? 4 : 1;
			it->value.colCount = (type == Type::MATRIX4X4) ? 4 : 1;
		}
		else
		{
			MaterialParam param;
			param.nameHash = nameHash;
			param.type = type;
			memcpy(param.value.data.data(), &value, sizeof(T));
			param.value.rowCount = (type == Type::MATRIX4X4) ? 4 : 1;
			param.value.colCount = (type == Type::MATRIX4X4) ? 4 : 1;
			m_params.emplace_back(std::move(param));
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
		SetOrAdd(nameHash, Type::FLOAT2, *reinterpret_cast<const XMFLOAT2*>(&v));
	}
	void MaterialParameterBlock::SetFloat3(size_t nameHash, const Vector3& v)
	{
		SetOrAdd(nameHash, Type::FLOAT3, *reinterpret_cast<const XMFLOAT3*>(&v));
	}
	void MaterialParameterBlock::SetFloat4(size_t nameHash, const Vector4& v)
	{
		SetOrAdd(nameHash, Type::FLOAT4, *reinterpret_cast<const XMFLOAT4*>(&v));
	}
	void MaterialParameterBlock::SetMatrix(size_t nameHash, const Matrix& m)
	{
		XMFLOAT4X4 xm;
		XMStoreFloat4x4(&xm, XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&m)));
    
		// D3D 默认是 row-major，cbuffer 中也是 row-major 存储
		// 所以我们可以直接拷贝 16 个 float
		SetOrAdd(nameHash, Type::MATRIX4X4, xm);
	}
	
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
		for (const auto& param : other.GetParams())
		{
			auto* existing = FindParam(param.nameHash);
			if (!existing || existing->type != param.type)
			{
				// 添加或替换
				*this.*([&](auto dummy) { this->SetOrAdd(param.nameHash, param.type, dummy); })(param.value.data);
			}
			else
			{
				if (!existing->value.Equals(param.value, param.type))
				{
					existing->value = param.value;
				}
			}
		}
	}
}