#pragma once
#include "lib/Utility/Helper.h"

namespace ElysiaRenderer
{
	class MaterialParameterBlock
	{
	public:
		enum Type
		{
			FLOAT,
			INT,
			UINT,
			BOOL,
			FLOAT2,
			FLOAT3,
			FLOAT4,
			MATRIX4X4,
			FloatArray,
			IntArray,
			Float2Array,
			Float3Array,
			Float4Array,
			MatrixArray
		};
		
		struct ParamValue
		{
			std::array<float, 16> data{}; // 最大支持 mat4x4
			uint32_t rowCount = 1;
			uint32_t colCount = 1;

			std::vector<float> arrayData{};

			bool operator==(const ParamValue& other) const;
			bool Equals(const ParamValue& other, Type type, float tolerance = 1e-6f) const;
		};
		
		struct MaterialParam
		{
			Type type;
			size_t nameHash;
			ParamValue value; 

			bool operator==(const MaterialParam& other) const
			{
				return nameHash == other.nameHash && value.Equals(other.value, type);
			}
		};
		
	public:
		void SetInt(size_t nameHash, int v);
		void SetUInt(size_t nameHash, unsigned int v);
		void SetFloat(size_t nameHash, float v);
		void SetFloat2(size_t nameHash, const Vector2& v);
		void SetFloat3(size_t nameHash, const Vector3& v);
		void SetFloat4(size_t nameHash, const Vector4& v);
		void SetMatrix(size_t nameHash, const Matrix& m);

		void SetFloatArray(size_t nameHash, const std::vector<float>& values);
		void SetIntArray(size_t nameHash, const std::vector<int>& values);
		void SetVector2Array(size_t nameHash, const std::vector<Vector2>& values);
		void SetVector3Array(size_t nameHash, const std::vector<Vector3>& values);
		void SetVector4Array(size_t nameHash, const std::vector<Vector4>& values);
		void SetMatrixArray(size_t nameHash, const std::vector<Matrix>& matrices);

		const std::vector<MaterialParam>& GetParams() const { return m_params; }
		
		const MaterialParam* FindParam(size_t nameHash) const;
		MaterialParam* FindParam(size_t nameHash);
		
		void RemoveParam(size_t nameHash);
		void Clear() { m_params.clear(); }
		void MergeFrom(const MaterialParameterBlock& other);

		// 设置脏状态改变回调
		void SetDirtyCallback(std::function<void()> callback);
		
		void MarkAsDirty();
		void ClearDirty();
		bool IsDirty() const;
		
	private:
		std::vector<MaterialParam> m_params;
		std::function<void()> m_dirtyCallback;
		bool m_isDirty = false;

		template<typename T>
		void SetOrAdd(size_t nameHash, Type type, const T& value);
		void SetValue(ParamValue& dst, float v);
		void SetValue(ParamValue& dst, int v);
		void SetValue(ParamValue& dst, unsigned int v);
		void SetValue(ParamValue& dst, const Vector2& v);
		void SetValue(ParamValue& dst, const Vector3& v);
		void SetValue(ParamValue& dst, const Vector4& v);
		void SetValue(ParamValue& dst, const Matrix& m);
		void SetValue(ParamValue& dst, const std::vector<float>& m);
		void SetValue(ParamValue& dst, const std::vector<int>& m);
		void SetValue(ParamValue& dst, const std::vector<Vector2>& m);
		void SetValue(ParamValue& dst, const std::vector<Vector3>& m);
		void SetValue(ParamValue& dst, const std::vector<Vector4>& m);
		void SetValue(ParamValue& dst, const std::vector<Matrix>& m);
		template<typename T>
		void SetOrAddArray(size_t nameHash, Type type, const T* values, size_t count);
	};

	
}

