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
			UInt,
			BOOL,
			FLOAT2,
			FLOAT3,
			FLOAT4,
			MATRIX4X4,
			FloatArray,
			IntArray,
			UIntArray,
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
		void SetInt(size_t nameHash, int v, size_t passID = 0);
		void SetUInt(size_t nameHash, unsigned int v, size_t passID = 0);
		void SetBool(size_t nameHash, bool v, size_t passID = 0);
		void SetFloat(size_t nameHash, float v, size_t passID = 0);
		void SetFloat2(size_t nameHash, const Vector2& v, size_t passID = 0);
		void SetFloat3(size_t nameHash, const Vector3& v, size_t passID = 0);
		void SetFloat4(size_t nameHash, const Vector4& v, size_t passID = 0);
		void SetMatrix(size_t nameHash, const Matrix& m, size_t passID = 0);

		void SetFloatArray(size_t nameHash, const std::vector<float>& values, size_t passID = 0);
		void SetIntArray(size_t nameHash, const std::vector<int>& values, size_t passID = 0);
		void SetUINTArray(size_t nameHash, const std::vector<UINT>& values, size_t passID = 0);
		void SetVector2Array(size_t nameHash, const std::vector<Vector2>& values, size_t passID = 0);
		void SetVector3Array(size_t nameHash, const std::vector<Vector3>& values, size_t passID = 0);
		void SetVector4Array(size_t nameHash, const std::vector<Vector4>& values, size_t passID = 0);
		void SetMatrixArray(size_t nameHash, const std::vector<Matrix>& matrices, size_t passID = 0);

		const std::vector<MaterialParam>& GetParams() const { return m_params; }
		
		const MaterialParam* FindParam(size_t nameHash, size_t passID = 0) const;
		MaterialParam* FindParam(size_t nameHash, size_t passID = 0);
		
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

		
		void SetValue(ParamValue& dst, float v);
		void SetValue(ParamValue& dst, int v);
		void SetValue(ParamValue& dst, unsigned int v);
		void SetValue(ParamValue& dst, const Vector2& v);
		void SetValue(ParamValue& dst, const Vector3& v);
		void SetValue(ParamValue& dst, const Vector4& v);
		void SetValue(ParamValue& dst, const Matrix& m);
		void SetValue(ParamValue& dst, const std::vector<float>& m);
		void SetValue(ParamValue& dst, const std::vector<int>& m);
		void SetValue(ParamValue& dst, const std::vector<UINT>& m);
		void SetValue(ParamValue& dst, const std::vector<Vector2>& m);
		void SetValue(ParamValue& dst, const std::vector<Vector3>& m);
		void SetValue(ParamValue& dst, const std::vector<Vector4>& m);
		void SetValue(ParamValue& dst, const std::vector<Matrix>& m);
		
		template<typename T>
		void SetOrAdd(size_t nameHash, Type type, const T& value);
		template<typename T>
		void SetOrAddArray(size_t nameHash, Type type, const std::vector<T>&  values);
		
		template<typename T>
		void SetOrAddWithPassID(size_t nameHash, Type type, const T& value, size_t passID = 0);
		template<typename T>
		void SetOrAddArrayWithPassID(size_t nameHash, Type type, const std::vector<T>&  values, size_t passID = 0);
		
		// -------------------------------------------------------
		// 核心混合函数：将 PassID 和 NameHash 合并
		// -------------------------------------------------------
		// 策略：如果 size_t 是 64位，使用 (PassID << 32) | NameHash
		//       如果 size_t 是 32位，使用哈希混合 (FNV-like)
		size_t MixPassAndName(size_t passID, size_t nameHash) const;
	};

	
}

