#pragma once
#include "stdafx.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace ElysiaRenderer
{
	extern class DX12Texture;
	enum class LoadTexType : uint8_t
	{
		Albedo,
		Normal,
		
	};
	struct LoadTexData
	{
		std::string m_path;
		//unsigned int m_ID;
		// tex type, such as albedo, normal
		LoadTexType m_texType;
	};

	struct DX12Material
	{
		DX12Material() = default;
		~DX12Material()
		{
			Destory();
		}
		DX12Material(const DX12Material& rhs) = default;
		DX12Material& operator=(DX12Material& rhs) = default;
		DX12Material(DX12Material&& rhs) = default;
		DX12Material& operator=(DX12Material&& rhs) noexcept
		{
			if (this != &rhs)
			{
				delete m_material;
				m_texData.clear();

				this->m_material = rhs.m_material;
				this->m_texData = rhs.m_texData;

				rhs.m_material = nullptr;
				rhs.m_texData.clear();
			}

			return *this;
		}

		void Destory()
		{
			if (m_material)
			{
				delete m_material;
				m_material = nullptr;
			}
			m_texData.clear();
		}

		aiMaterial* m_material;
		std::vector<LoadTexData*> m_texData{};
	};
}