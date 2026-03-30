#pragma once
#include "Programs/Helper.h"

namespace ElysiaRenderer
{
    class Jitter
    {
    public:
        enum class Type : int
        {
            Default = 0,
            Uniform2,
            Uniform4,
            Uniform4Helix,
            Rotated4,
            Rotated4Helix,
            Halton23X8,
            Halton23X16,
            Halton23X32,
            Halton23X64,
            Count
        };
        NLOHMANN_JSON_SERIALIZE_ENUM(Type,
                                     {
                                     {Type::Default,
                                     "Default"},
                                     {Type::Uniform2,
                                     "Uniform2"},
                                     {Type::Uniform4,
                                     "Uniform4"},
                                     {Type::Uniform4Helix,
                                     "Uniform4Helix"},
                                     {Type::Rotated4,
                                     "Rotated4"},
                                     {Type::Rotated4Helix,
                                     "Rotated4Helix"},
                                     {Type::Halton23X8,
                                     "Halton23X8"},
                                     {Type::Halton23X16,
                                     "Halton23X16"},
                                     {Type::Halton23X32,
                                     "Halton23X32"},
                                     {Type::Halton23X64,
                                     "Halton23X64"},
                                     {Type::Count,
                                     "Count"}
                                     })

        /**
         * @brief 获取当前类型的抖动偏移并递增索引
         */
        static Vector2 SampleJitterUV(Type sampleType)
        {
            const auto& list = m_samplerData[static_cast<int>(sampleType)];

            // 自动循环索引
            int index = m_sampleIndex % static_cast<int>(list.size());
            Vector2 res = list[index];

            m_sampleIndex ++;
            return res;
        }

        /**
         * @brief 重置采样索引，通常在 TAA 重新启动或每帧开始时可选调用
         */
        static void ResetIndex()
        {
            m_sampleIndex = 0;
        }

    private
    :
        static float CalculateHalton(int base, int index)
        {
            index ++;
            float f = 1.0f;
            float r = 0.0f;
            while (index > 0)
            {
                f /= static_cast<float>(base);
                r += f * static_cast<float>(index % base);
                index = static_cast<int>(std::floor(static_cast<float>(index) / static_cast<float>(base)));
            }
            return r;
        }
        static std::vector<Vector2> GenerateHalton(int count, int baseX, int baseY)
        {
            std::vector<Vector2> seq;
            seq.reserve(count);
            for (int i = 0; i < count; i ++)
            {
                seq.push_back(Vector2(
                    CalculateHalton(baseX, i) - 0.5f,
                    CalculateHalton(baseY, i) - 0.5f
                    ));
            }
            return seq;
        }

        // 存储所有预计算好的采样点
        static inline const std::vector<Vector2> m_samplerData[static_cast<int>(Type::Count)]
        {
            {Vector2(0.0f, 0.0f)},                            // Default
            {Vector2(-0.25f, -0.25f), Vector2(0.25f, 0.25f)}, // Uniform2
            {
                Vector2(-0.25f, -0.25f), Vector2(0.25f, -0.25f),
                Vector2(0.25f, 0.25f), Vector2(-0.25f, 0.25f)
            }, // Uniform4
            {
                Vector2(-0.25f, -0.25f), Vector2(0.25f, 0.25f),
                Vector2(0.25f, -0.25f), Vector2(-0.25f, 0.25f)
            }, // Uniform4Helix
            {
                Vector2(-0.125f, -0.375f), Vector2(0.375f, -0.125f),
                Vector2(0.125f, 0.375f), Vector2(-0.375f, 0.125f)
            }, // Rotated4
            {
                Vector2(-0.125f, -0.375f), Vector2(0.125f, 0.375f),
                Vector2(0.375f, -0.125f), Vector2(-0.375f, 0.125f)
            },                        // Rotated4Helix
            GenerateHalton(8, 2, 3),  // Halton23X8
            GenerateHalton(16, 2, 3), // Halton23X16
            GenerateHalton(32, 2, 3), // Halton23X32
            GenerateHalton(64, 2, 3)  // Halton23X64
        };

        // 使用 thread_local 确保多线程录制 CommandList 时索引独立且安全
        static inline thread_local int m_sampleIndex = 0;
    };

    struct TAAParameter
    {
        bool Enable;
        Jitter::Type jitterType = Jitter::Type::Halton23X8;
        float jitterIntensity = 1.f;
        float staticWeight = 0.95f;
        float dynamicWeight = 0.1f;
        float maxWeight = 0.5f;
        float sampleRate = 1.0f;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TAAParameter,
                                                    Enable,
                                                    jitterType,
                                                    jitterIntensity,
                                                    staticWeight,
                                                    dynamicWeight,
                                                    maxWeight,
                                                    sampleRate)
}