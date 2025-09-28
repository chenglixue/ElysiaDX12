#pragma once
#include "stdafx.h"
#include "DX12Vertex.h"
#include "BoundingBox.h"

namespace ElysiaRenderer
{
    using namespace ElysiaHelper;

    enum
    {
        attrib_0 = 0,
        attrib_1 = 1,
        attrib_2 = 2,
        attrib_3 = 3,
        attrib_4 = 4,
        attrib_5 = 5,
        attrib_6 = 6,
        attrib_7 = 7,
        attrib_8 = 8,
        attrib_9 = 9,
        attrib_10 = 10,
        attrib_11 = 11,
        attrib_12 = 12,
        attrib_13 = 13,
        attrib_14 = 14,
        attrib_15 = 15,

        // friendly name aliases
        attrib_position = attrib_0,
        attrib_texcoord0 = attrib_1,
        attrib_normal = attrib_2,
        attrib_tangent = attrib_3,
        attrib_bitangent = attrib_4,

        maxAttribs = 16
    };

    enum
    {
        attrib_mask_0 = (1 << 0),
        attrib_mask_1 = (1 << 1),
        attrib_mask_2 = (1 << 2),
        attrib_mask_3 = (1 << 3),
        attrib_mask_4 = (1 << 4),
        attrib_mask_5 = (1 << 5),
        attrib_mask_6 = (1 << 6),
        attrib_mask_7 = (1 << 7),
        attrib_mask_8 = (1 << 8),
        attrib_mask_9 = (1 << 9),
        attrib_mask_10 = (1 << 10),
        attrib_mask_11 = (1 << 11),
        attrib_mask_12 = (1 << 12),
        attrib_mask_13 = (1 << 13),
        attrib_mask_14 = (1 << 14),
        attrib_mask_15 = (1 << 15),

        // friendly name aliases
        attrib_mask_position = attrib_mask_0,
        attrib_mask_texcoord0 = attrib_mask_1,
        attrib_mask_normal = attrib_mask_2,
        attrib_mask_tangent = attrib_mask_3,
        attrib_mask_bitangent = attrib_mask_4,
    };

    enum
    {
        attrib_format_none = 0,
        attrib_format_ubyte,
        attrib_format_byte,
        attrib_format_ushort,
        attrib_format_short,
        attrib_format_float,

        attrib_formats
    };

    struct Attrib
    {
        UINT16 offset; // byte offset from the start of the vertex
        UINT16 normalized; // if true, integer formats are interpreted as [-1, 1] or [0, 1]
        UINT16 components; // 1-4
        UINT16 format;
    };

	struct Mesh
	{
		std::string name;

		UINT materialIndex = 0;

		UINT attribsEnabled;
		uint32_t vertexStride;

        Attrib attrib[maxAttribs];

        AxisAlignedBox boundingBox;

		UINT vertexDataOffset = 0;
        UINT vertexCount = 0;
		UINT indexDataOffset = 0;
        UINT indexCount = 0;
	};

	struct MeshData
	{
		uint32_t meshCount;
		uint32_t materialCount;
		uint32_t vertexDataByteSize;
		uint32_t indexDataByteSize;

        AxisAlignedBox boundingBox;
	};
}