#include "stdafx.h"

#include "../public/ConfigCache.h"
#include "../public/UserData.h"

#include "Programs/public/Helper.h"
#include "Programs/public/Log.h"

#include <shellapi.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace ElysiaRenderer
{
    namespace
    {
        std::string Trim(std::string text)
        {
            auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
            while (!text.empty() && isSpace(static_cast<unsigned char>(text.front())))
                text.erase(text.begin());
            while (!text.empty() && isSpace(static_cast<unsigned char>(text.back())))
                text.pop_back();
            return text;
        }

        std::string Lower(std::string text)
        {
            for (char& c : text)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return text;
        }

        std::string MakeKey(const std::string& section, const std::string& name)
        {
            return Lower(section) + "/" + Lower(name);
        }

        std::filesystem::path ExeDirectory()
        {
            WCHAR path[512] = {};
            ElysiaHelper::GetAssetsPath(path, _countof(path));
            return std::filesystem::path(path);
        }

        std::string WideToUtf8(const std::wstring& text)
        {
            if (text.empty())
                return {};
            int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (size <= 1)
                return {};
            std::string out(static_cast<size_t>(size - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, out.data(), size, nullptr, nullptr);
            return out;
        }

        std::wstring Utf8ToWide(const std::string& text)
        {
            if (text.empty())
                return {};
            int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            if (size <= 1)
                return {};
            std::wstring out(static_cast<size_t>(size - 1), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
            return out;
        }

        std::string FormatFloat(float value)
        {
            char buf[64] = {};
            snprintf(buf, sizeof(buf), "%.9g", value);
            return buf;
        }

        std::string FormatVector3(const Vector3& value)
        {
            return "(X=" + FormatFloat(value.x) + ",Y=" + FormatFloat(value.y) + ",Z=" + FormatFloat(value.z) + ")";
        }

        bool ParseBool(const std::string& text, bool& out)
        {
            const std::string lower = Lower(text);
            if (lower == "true" || lower == "1" || lower == "yes")
            {
                out = true;
                return true;
            }
            if (lower == "false" || lower == "0" || lower == "no")
            {
                out = false;
                return true;
            }
            return false;
        }

        bool ParseFloat(const std::string& text, float& out)
        {
            char* end = nullptr;
            const float value = std::strtof(text.c_str(), &end);
            if (end == text.c_str())
                return false;
            out = value;
            return true;
        }

        bool ParseInt(const std::string& text, int& out)
        {
            char* end = nullptr;
            const long value = std::strtol(text.c_str(), &end, 10);
            if (end == text.c_str())
                return false;
            out = static_cast<int>(value);
            return true;
        }

        bool Component(const std::string& text, char axis, float& out)
        {
            const std::string lower = Lower(text);
            const std::string token = std::string(1, static_cast<char>(std::tolower(static_cast<unsigned char>(axis)))) + "=";
            const size_t pos = lower.find(token);
            if (pos == std::string::npos)
                return false;
            return ParseFloat(text.substr(pos + token.size()), out);
        }

        bool ParseVector3(const std::string& text, Vector3& out)
        {
            Vector3 value = out;
            if (!Component(text, 'X', value.x) || !Component(text, 'Y', value.y) || !Component(text, 'Z', value.z))
                return false;
            out = value;
            return true;
        }

        template <typename Enum>
        struct EnumName
        {
            Enum value;
            const char* name;
        };

        template <typename Enum, size_t Count>
        bool ParseEnum(const std::string& text, const EnumName<Enum> (&table)[Count], Enum& out)
        {
            for (const auto& entry : table)
            {
                if (_stricmp(text.c_str(), entry.name) == 0)
                {
                    out = entry.value;
                    return true;
                }
            }
            return false;
        }

        template <typename Enum, size_t Count>
        const char* NameOf(Enum value, const EnumName<Enum> (&table)[Count])
        {
            for (const auto& entry : table)
            {
                if (entry.value == value)
                    return entry.name;
            }
            return table[0].name;
        }

        const EnumName<ShadingModel> kShadingModels[] =
        {
            {ShadingModel::Unlit, "Unlit"},
            {ShadingModel::DefaultLit, "Default Lit"},
            {ShadingModel::Preintegrated_Skin, "Preintegrated Skin"},
            {ShadingModel::Subsurface_Profile, "Subsurface Profile"},
            {ShadingModel::Hair, "Hair"},
            {ShadingModel::Eye, "Eye"},
            {ShadingModel::Cloth, "Cloth"},
            {ShadingModel::Clear_Coat, "Clear Coat"},
            {ShadingModel::Two_Sided_Foliage, "Two Sided Foliage"},
        };

        const EnumName<ShadowType> kShadowTypes[] =
        {
            {ShadowType::Hard, "Hard"},
            {ShadowType::Soft, "Soft"},
        };

        const EnumName<ShadowQuality> kShadowQualities[] =
        {
            {ShadowQuality::Low, "Low"},
            {ShadowQuality::Middle, "Medium"},
            {ShadowQuality::High, "High"},
            {ShadowQuality::VeryHigh, "VeryHigh"},
        };

        const EnumName<HDRQuality> kHdrQualities[] =
        {
            {HDRQuality::Low, "Low"},
            {HDRQuality::High, "High"},
        };

        const EnumName<TonemapMode> kTonemapModes[] =
        {
            {TonemapMode::Neutral, "Neutral"},
            {TonemapMode::LMP, "LMP"},
            {TonemapMode::AMD, "AMD"},
            {TonemapMode::ACESFilm, "ACESFilm"},
            {TonemapMode::Uncharted2, "Uncharted2"},
            {TonemapMode::DX11DSK, "DX11DSK"},
        };

        const EnumName<ColorSpace> kColorSpaces[] =
        {
            {ColorSpace::ColorSpace_REC709, "ColorSpace_REC709"},
            {ColorSpace::ColorSpace_P3, "ColorSpace_P3"},
            {ColorSpace::ColorSpace_REC2020, "ColorSpace_REC2020"},
            {ColorSpace::ColorSpace_Display, "ColorSpace_Display"},
        };

        const EnumName<CAULDRON_DX12::DisplayMode> kDisplayModes[] =
        {
            {CAULDRON_DX12::DisplayMode::DISPLAYMODE_SDR, "DISPLAYMODE_SDR"},
            {CAULDRON_DX12::DisplayMode::DISPLAYMODE_FSHDR_Gamma22, "DISPLAYMODE_FSHDR_Gamma22"},
            {CAULDRON_DX12::DisplayMode::DISPLAYMODE_FSHDR_SCRGB, "DISPLAYMODE_FSHDR_SCRGB"},
            {CAULDRON_DX12::DisplayMode::DISPLAYMODE_HDR10_2084, "DISPLAYMODE_HDR10_2084"},
            {CAULDRON_DX12::DisplayMode::DISPLAYMODE_HDR10_SCRGB, "DISPLAYMODE_HDR10_SCRGB"},
        };

        const EnumName<AOBlurQuality> kAoBlurQualities[] =
        {
            {AOBlurQuality::Low, "Low"},
            {AOBlurQuality::Middle, "Middle"},
            {AOBlurQuality::High, "High"},
        };

        const EnumName<AODebugTarget> kAoDebugTargets[] =
        {
            {AODebugTarget::Importance, "Importance"},
            {AODebugTarget::HIZMipmap, "HIZMipmap"},
            {AODebugTarget::AO, "AO"},
        };

        const EnumName<DebugMode> kDebugModes[] =
        {
            {DebugMode::None, "None"},
            {DebugMode::AO, "AO"},
            {DebugMode::GIProbe, "GIProbe"},
            {DebugMode::Normal, "Normal"},
            {DebugMode::AABB, "AABB"},
            {DebugMode::Bloom, "Bloom"},
            {DebugMode::Velocity, "Velocity"},
            {DebugMode::GI, "GI"},
            {DebugMode::ShadowMask, "ShadowMask"},
            {DebugMode::Albedo, "Albedo"},
            {DebugMode::Emission, "Emission"},
            {DebugMode::Metallic, "Metallic"},
            {DebugMode::Roughness, "Roughness"},
        };

        const EnumName<DebugDownOrUp> kBloomDirections[] =
        {
            {DebugDownOrUp::Down, "Down"},
            {DebugDownOrUp::Up, "Up"},
        };

        const EnumName<Jitter::Type> kJitterTypes[] =
        {
            {Jitter::Type::Default, "Default"},
            {Jitter::Type::Uniform2, "Uniform2"},
            {Jitter::Type::Uniform4, "Uniform4"},
            {Jitter::Type::Uniform4Helix, "Uniform4Helix"},
            {Jitter::Type::Rotated4, "Rotated4"},
            {Jitter::Type::Rotated4Helix, "Rotated4Helix"},
            {Jitter::Type::Halton23X8, "Halton23X8"},
            {Jitter::Type::Halton23X16, "Halton23X16"},
            {Jitter::Type::Halton23X32, "Halton23X32"},
            {Jitter::Type::Halton23X64, "Halton23X64"},
        };

        constexpr const char* kFallbackModel = "glTF\\Sponza\\Sponza.gltf";
    }

    ConfigCache& ConfigCache::Get()
    {
        static ConfigCache cache;
        return cache;
    }

    bool ConfigCache::TryGet(const char* section, const char* key, std::string& out) const
    {
        const auto it = m_scalars.find(MakeKey(section, key));
        if (it == m_scalars.end())
            return false;
        out = it->second;
        return true;
    }

    bool ConfigCache::TryGetBaseline(const char* section, const char* key, std::string& out) const
    {
        const auto it = m_baselineScalars.find(MakeKey(section, key));
        if (it == m_baselineScalars.end())
            return false;
        out = it->second;
        return true;
    }

    const std::vector<std::string>* ConfigCache::TryGetArray(const char* section, const char* key) const
    {
        const auto it = m_arrays.find(MakeKey(section, key));
        if (it == m_arrays.end())
            return nullptr;
        return &it->second;
    }

    const std::vector<std::string>* ConfigCache::TryGetBaselineArray(const char* section, const char* key) const
    {
        const auto it = m_baselineArrays.find(MakeKey(section, key));
        if (it == m_baselineArrays.end())
            return nullptr;
        return &it->second;
    }

    void ConfigCache::Ingest(const std::string& raw,
                             std::string& section,
                             std::unordered_set<std::string>& dotted)
    {
        const std::string line = Trim(raw);
        if (line.empty() || line.front() == ';' || line.front() == '#')
            return;

        if (line.front() == '[' && line.back() == ']')
        {
            section = Trim(line.substr(1, line.size() - 2));
            return;
        }

        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            ElysiaHelper::Log::Warn("Config: ignored line '%s'", line.c_str());
            return;
        }

        std::string name = Trim(line.substr(0, eq));
        const std::string value = Trim(line.substr(eq + 1));
        if (section.empty() || name.empty())
            return;

        char op = 0;
        if (name.front() == '+' || name.front() == '-' || name.front() == '.')
        {
            op = name.front();
            name = Trim(name.substr(1));
        }

        const std::string id = MakeKey(section, name);
        if (op == 0)
        {
            m_scalars[id] = value;
            return;
        }

        auto& list = m_arrays[id];
        if (op == '.')
        {
            if (dotted.insert(id).second)
                list.clear();
            if (!value.empty())
                list.push_back(value);
        }
        else if (op == '+')
        {
            if (!value.empty())
                list.push_back(value);
        }
        else
        {
            list.erase(std::remove(list.begin(), list.end(), value), list.end());
        }
    }

    void ConfigCache::LoadFile(const std::filesystem::path& path, bool required)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            if (required)
                ElysiaHelper::Log::Error("Config: missing %s", path.string().c_str());
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string text = buffer.str();
        if (text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF)
        {
            text.erase(0, 3);
        }

        std::string section;
        std::unordered_set<std::string> dotted;
        std::string line;
        std::istringstream lines(text);
        while (std::getline(lines, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            Ingest(line, section, dotted);
        }

        ElysiaHelper::Log::Info("Config: loaded %s", path.string().c_str());
    }

    void ConfigCache::LoadCommandLine()
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv == nullptr)
            return;

        const std::wstring prefix = L"-ini:Engine:";
        std::unordered_set<std::string> dotted;
        for (int i = 0; i < argc; ++i)
        {
            const std::wstring arg = argv[i];
            if (arg.rfind(prefix, 0) != 0)
                continue;

            const std::string spec = WideToUtf8(arg.substr(prefix.size()));
            if (spec.size() < 4 || spec.front() != '[')
                continue;
            const size_t close = spec.find(']');
            const size_t colon = close == std::string::npos ? std::string::npos : spec.find(':', close);
            if (close == std::string::npos || colon == std::string::npos)
                continue;

            std::string section = spec.substr(1, close - 1);
            Ingest(spec.substr(colon + 1), section, dotted);
            ElysiaHelper::Log::Info("Config: command line %s", spec.c_str());
        }
        LocalFree(argv);
    }

    void ConfigCache::LoadHierarchy()
    {
        m_scalars.clear();
        m_arrays.clear();
        m_baselineScalars.clear();
        m_baselineArrays.clear();

        const std::filesystem::path exe = ExeDirectory();
        LoadFile(exe / "Config" / "BaseEngine.ini", true);
        LoadFile(exe / "Config" / "DefaultEngine.ini", false);

        m_baselineScalars = m_scalars;
        m_baselineArrays = m_arrays;

        LoadFile(exe / "Saved" / "Config" / "Windows" / "Engine.ini", false);
        LoadCommandLine();
    }

    void ConfigCache::Apply()
    {
        auto& data = UserData::GetInstance();

        data.hdrParameter.bShoulder = false;
        data.hdrParameter.SoftGap = 0.f;
        data.hdrParameter.HdrMax = 256.f;
        data.hdrParameter.LpmExposure = 8.f;
        data.hdrParameter.Contrast = 0.25f;
        data.hdrParameter.ShoulderContrast = 1.f;
        data.hdrParameter.Saturation = Vector3::Zero;
        data.hdrParameter.Crosstalk = Vector3(1.f, 0.5f, 1.f / 32.f);
        data.taaParameter.Enable = true;
        data.sharpenParameter.enable = false;
        data.sharpenParameter.sharpen = 0.f;
        data.GIParameter.enableLine = false;
        data.GIParameter.bHideInactiveProbe = false;
        data.GIParameter.bTextureVisualization = false;
        data.GIParameter.lineWidth = 1.f;
        data.GIParameter.normalBias = 0.f;
        data.GIParameter.viewBias = 0.f;
        data.GIParameter.probeIrradianceThreshold = 0.f;
        data.GIParameter.probeBrightnessThreshold = 0.f;
        data.bloomParameter.debugMode = DebugDownOrUp::Down;

        auto warn = [](const char* section, const char* key, const std::string& text)
        {
            ElysiaHelper::Log::Warn("Config: %s/%s has invalid value '%s'", section, key, text.c_str());
        };

        auto setBool = [&](const char* section, const char* key, bool& dst)
        {
            std::string text;
            if (!TryGet(section, key, text))
                return;
            bool value = false;
            if (!ParseBool(text, value))
            {
                warn(section, key, text);
                return;
            }
            dst = value;
        };
        auto setFloat = [&](const char* section, const char* key, float& dst)
        {
            std::string text;
            if (!TryGet(section, key, text))
                return;
            float value = 0.f;
            if (!ParseFloat(text, value))
            {
                warn(section, key, text);
                return;
            }
            dst = value;
        };
        auto setInt = [&](const char* section, const char* key, int& dst)
        {
            std::string text;
            if (!TryGet(section, key, text))
                return;
            int value = 0;
            if (!ParseInt(text, value))
            {
                warn(section, key, text);
                return;
            }
            dst = value;
        };
        auto setVector = [&](const char* section, const char* key, Vector3& dst)
        {
            std::string text;
            if (!TryGet(section, key, text))
                return;
            if (!ParseVector3(text, dst))
                warn(section, key, text);
        };
        auto setEnum = [&](const char* section, const char* key, auto& dst, const auto& table)
        {
            std::string text;
            if (!TryGet(section, key, text))
                return;
            if (!ParseEnum(text, table, dst))
                warn(section, key, text);
        };

        setVector("Light", "Color", data.lightColor);
        setVector("Light", "Direction", data.lightDir);
        setFloat("Light", "Intensity", data.lightIntensity);

        setEnum("Material", "ShadingModel", data.shadingModelID, kShadingModels);
        setVector("Material", "BaseColorTint", data.BaseColorTint);
        setFloat("Material", "Opacity", data.Opacity);
        setFloat("Material", "Cutoff", data.Cutoff);
        setFloat("Material", "NormalIntensity", data.NormalIntensity);
        setFloat("Material", "MetallicIntensity", data.MetallicIntensity);
        setFloat("Material", "RoughnessIntensity", data.RoughnessIntensity);
        setFloat("Material", "AmbientCubemapIntensity", data.AmbientCubemapIntensity);
        setFloat("Material", "Specular", data.Specular);
        setVector("Material", "AmbientCubemapTint", data.AmbientCubemapTint);
        setVector("Material", "EmissionTint", data.EmissionTint);

        setFloat("Subsurface", "CurveScale", data.subsurfaceScatterParameter.CurveScale);
        setFloat("Subsurface", "MinCurve", data.subsurfaceScatterParameter.MinCurve);
        setVector("Subsurface", "SubsurfaceColor", data.subsurfaceScatterParameter.SubsurfaceColor);
        setFloat("Subsurface", "ScatterRadius", data.subsurfaceScatterParameter.ScatterRadius);
        setFloat("Subsurface", "TransmissionScale", data.subsurfaceScatterParameter.TransmissionScale);
        setFloat("Subsurface", "TransmissionRange", data.subsurfaceScatterParameter.TransmissionRange);
        setFloat("Subsurface", "TransmissionEdgeGlow", data.subsurfaceScatterParameter.TransmissionEdgeGlow);

        setBool("Hair", "EnableMultiScatter", data.hairParameter.bEnableMultiScatter);
        setBool("Hair", "EnableR", data.hairParameter.bEnableR);
        setBool("Hair", "EnableTT", data.hairParameter.bEnableTT);
        setBool("Hair", "EnableTRT", data.hairParameter.bEnableTRT);
        setFloat("Hair", "BackLit", data.hairParameter.backLit);

        setEnum("Shadow", "Type", data.shadowParameter.shadowType, kShadowTypes);
        setEnum("Shadow", "Quality", data.shadowParameter.shadowQuality, kShadowQualities);
        setFloat("Shadow", "DepthBias", data.shadowParameter.shadowDepthBias);
        setFloat("Shadow", "SlopeDepthBias", data.shadowParameter.shadowSlopeDepthBias);
        setFloat("Shadow", "MaxSlopeDepthBias", data.shadowParameter.shadowMaxSlopeDepthBias);
        setFloat("Shadow", "Radius", data.shadowParameter.shadowRadius);
        setBool("Shadow", "Enable", data.shadowParameter.EnableShadow);
        setBool("Shadow", "EnableTAA", data.shadowParameter.EnableTAA);

        setBool("HDR", "Enable", data.hdrParameter.IsUseHDR);
        setEnum("HDR", "Quality", data.hdrParameter.HDRLevel, kHdrQualities);
        setEnum("HDR", "Tonemap", data.hdrParameter.tonemapMode, kTonemapModes);
        setEnum("HDR", "ColorSpace", data.hdrParameter.colorSpace, kColorSpaces);
        setEnum("HDR", "DisplayMode", data.hdrParameter.displayMode, kDisplayModes);
        setFloat("HDR", "LocalExposure", data.hdrParameter.localExposure);
        setBool("HDR", "Shoulder", data.hdrParameter.bShoulder);
        setFloat("HDR", "SoftGap", data.hdrParameter.SoftGap);
        setFloat("HDR", "HdrMax", data.hdrParameter.HdrMax);
        setFloat("HDR", "Exposure", data.hdrParameter.LpmExposure);
        setFloat("HDR", "Contrast", data.hdrParameter.Contrast);
        setFloat("HDR", "ShoulderContrast", data.hdrParameter.ShoulderContrast);
        setVector("HDR", "Saturation", data.hdrParameter.Saturation);
        setVector("HDR", "Crosstalk", data.hdrParameter.Crosstalk);

        setBool("AO", "Enable", data.aoParameter.IsEnableAO);
        setFloat("AO", "Radius", data.aoParameter.Radius);
        setFloat("AO", "FadeRadius", data.aoParameter.FadeRadius);
        setFloat("AO", "FadeDistance", data.aoParameter.FadeDistance);
        setFloat("AO", "Bias", data.aoParameter.Bias);
        setFloat("AO", "IntensityMul", data.aoParameter.IntensityMul);
        setFloat("AO", "IntensityPow", data.aoParameter.IntensityPow);
        setBool("AO", "Lerp", data.aoParameter.IsLerpAO);
        setFloat("AO", "TAALerpFactor", data.aoParameter.TAALerpFactor);
        setBool("AO", "Blur", data.aoParameter.IsBlur);
        setInt("AO", "BlurCount", data.aoParameter.BlurCount);
        setEnum("AO", "BlurQuality", data.aoParameter.BlurQuality, kAoBlurQualities);
        setInt("AO", "BlurIntensity", data.aoParameter.BlurIntensity);
        setFloat("AO", "Sharpness", data.aoParameter.Sharpness);
        setEnum("AO", "DebugTarget", data.aoParameter.debugTarget, kAoDebugTargets);
        setFloat("AO", "HIZMipFactor", data.aoParameter.HIZMipFactor);
        setInt("AO", "HIZMipmap", data.aoParameter.HIZMipmap);
        setBool("AO", "TAA", data.aoParameter.IsTAA);
        setFloat("AO", "ImportanceIntensity", data.aoParameter.importanceIntensity);
        setFloat("AO", "HIZRadius", data.aoParameter.HIZRadius);

        setBool("GI", "EnableLine", data.GIParameter.enableLine);
        setBool("GI", "HideInactiveProbe", data.GIParameter.bHideInactiveProbe);
        setBool("GI", "TextureVisualization", data.GIParameter.bTextureVisualization);
        setFloat("GI", "LineWidth", data.GIParameter.lineWidth);
        setFloat("GI", "NormalBias", data.GIParameter.normalBias);
        setFloat("GI", "ViewBias", data.GIParameter.viewBias);
        setFloat("GI", "BlendWeight", data.GIParameter.blendWeight);
        setFloat("GI", "Gamma", data.GIParameter.gamma);
        setFloat("GI", "ProbeIrradianceThreshold", data.GIParameter.probeIrradianceThreshold);
        setFloat("GI", "ProbeBrightnessThreshold", data.GIParameter.probeBrightnessThreshold);
        setVector("GI", "ProbeGroupOrigin", data.GIParameter.probeGroupOrigin);

        setEnum("Debug", "Mode", data.debugMode, kDebugModes);
        setInt("Debug", "MipmapLevel", data.mipmapLevel);
        setInt("Debug", "InstanceID", data.instanceID);
        setBool("Debug", "EnableHIZ", data.EnableHIZ);

        setBool("Bloom", "Enable", data.bloomParameter.enable);
        setFloat("Bloom", "Radius", data.bloomParameter.radius);
        setFloat("Bloom", "Intensity", data.bloomParameter.intensity);
        setInt("Bloom", "Mipmap", data.bloomParameter.mipmap);
        setEnum("Bloom", "DebugDirection", data.bloomParameter.debugMode, kBloomDirections);

        setBool("TAA", "Enable", data.taaParameter.Enable);
        setEnum("TAA", "Jitter", data.taaParameter.jitterType, kJitterTypes);
        setFloat("TAA", "JitterIntensity", data.taaParameter.jitterIntensity);
        setFloat("TAA", "StaticWeight", data.taaParameter.staticWeight);
        setFloat("TAA", "DynamicWeight", data.taaParameter.dynamicWeight);
        setFloat("TAA", "MaxWeight", data.taaParameter.maxWeight);
        setFloat("TAA", "SampleRate", data.taaParameter.sampleRate);

        setBool("Sharpen", "Enable", data.sharpenParameter.enable);
        setFloat("Sharpen", "Amount", data.sharpenParameter.sharpen);

        setFloat("SSSR", "RoughnessThreshold", data.sssrParameter.roughnessThreshold);
        int samplesPerQuad = static_cast<int>(data.sssrParameter.samplesPerQuad);
        setInt("SSSR", "SamplesPerQuad", samplesPerQuad);
        if (samplesPerQuad < 0)
            samplesPerQuad = 0;
        data.sssrParameter.samplesPerQuad = static_cast<UINT>(samplesPerQuad);

        if (const auto* paths = TryGetArray("Startup", "ModelPath"))
        {
            g_ModelPaths.clear();
            for (const std::string& path : *paths)
                g_ModelPaths.push_back(Utf8ToWide(path));
        }
        else if (g_ModelPaths.empty())
        {
            g_ModelPaths.emplace_back(Utf8ToWide(kFallbackModel));
        }
    }

    void ConfigCache::SaveDiff() const
    {
        std::ostringstream out;
        out << "; User overrides. Only keys that differ from BaseEngine.ini + DefaultEngine.ini.\n";
        std::string currentSection;

        auto begin = [&](const char* section)
        {
            if (currentSection == section)
                return;
            if (!currentSection.empty())
                out << "\n";
            currentSection = section;
            out << "[" << section << "]\n";
        };

        auto emitBool = [&](const char* section, const char* key, bool live)
        {
            std::string baseline;
            if (!TryGetBaseline(section, key, baseline))
                return;
            bool parsed = false;
            if (ParseBool(baseline, parsed) && parsed == live)
                return;
            begin(section);
            out << key << "=" << (live ? "true" : "false") << "\n";
        };
        auto emitFloat = [&](const char* section, const char* key, float live)
        {
            std::string baseline;
            if (!TryGetBaseline(section, key, baseline))
                return;
            float parsed = 0.f;
            if (ParseFloat(baseline, parsed) && parsed == live)
                return;
            begin(section);
            out << key << "=" << FormatFloat(live) << "\n";
        };
        auto emitInt = [&](const char* section, const char* key, int live)
        {
            std::string baseline;
            if (!TryGetBaseline(section, key, baseline))
                return;
            int parsed = 0;
            if (ParseInt(baseline, parsed) && parsed == live)
                return;
            begin(section);
            out << key << "=" << live << "\n";
        };
        auto emitVector = [&](const char* section, const char* key, const Vector3& live)
        {
            std::string baseline;
            if (!TryGetBaseline(section, key, baseline))
                return;
            Vector3 parsed = live;
            if (ParseVector3(baseline, parsed) && parsed.x == live.x && parsed.y == live.y && parsed.z == live.z)
                return;
            begin(section);
            out << key << "=" << FormatVector3(live) << "\n";
        };
        auto emitEnum = [&](const char* section, const char* key, auto live, const auto& table)
        {
            std::string baseline;
            if (!TryGetBaseline(section, key, baseline))
                return;
            decltype(live) parsed = live;
            if (ParseEnum(baseline, table, parsed) && parsed == live)
                return;
            begin(section);
            out << key << "=" << NameOf(live, table) << "\n";
        };

        const auto& data = UserData::GetInstance();

        std::vector<std::string> livePaths;
        livePaths.reserve(g_ModelPaths.size());
        for (const std::wstring& path : g_ModelPaths)
            livePaths.push_back(WideToUtf8(path));
        const std::vector<std::string>* baselinePaths = TryGetBaselineArray("Startup", "ModelPath");
        const bool pathsMatch = baselinePaths != nullptr && *baselinePaths == livePaths;
        const bool fallbackOnly = baselinePaths == nullptr && livePaths.size() == 1 && livePaths[0] == kFallbackModel;
        if (!pathsMatch && !fallbackOnly)
        {
            begin("Startup");
            if (livePaths.empty())
            {
                out << ".ModelPath=\n";
            }
            else
            {
                for (size_t i = 0; i < livePaths.size(); ++i)
                    out << (i == 0 ? "." : "+") << "ModelPath=" << livePaths[i] << "\n";
            }
        }

        emitVector("Light", "Color", data.lightColor);
        emitVector("Light", "Direction", data.lightDir);
        emitFloat("Light", "Intensity", data.lightIntensity);

        emitEnum("Material", "ShadingModel", data.shadingModelID, kShadingModels);
        emitVector("Material", "BaseColorTint", data.BaseColorTint);
        emitFloat("Material", "Opacity", data.Opacity);
        emitFloat("Material", "Cutoff", data.Cutoff);
        emitFloat("Material", "NormalIntensity", data.NormalIntensity);
        emitFloat("Material", "MetallicIntensity", data.MetallicIntensity);
        emitFloat("Material", "RoughnessIntensity", data.RoughnessIntensity);
        emitFloat("Material", "AmbientCubemapIntensity", data.AmbientCubemapIntensity);
        emitFloat("Material", "Specular", data.Specular);
        emitVector("Material", "AmbientCubemapTint", data.AmbientCubemapTint);
        emitVector("Material", "EmissionTint", data.EmissionTint);

        emitFloat("Subsurface", "CurveScale", data.subsurfaceScatterParameter.CurveScale);
        emitFloat("Subsurface", "MinCurve", data.subsurfaceScatterParameter.MinCurve);
        emitVector("Subsurface", "SubsurfaceColor", data.subsurfaceScatterParameter.SubsurfaceColor);
        emitFloat("Subsurface", "ScatterRadius", data.subsurfaceScatterParameter.ScatterRadius);
        emitFloat("Subsurface", "TransmissionScale", data.subsurfaceScatterParameter.TransmissionScale);
        emitFloat("Subsurface", "TransmissionRange", data.subsurfaceScatterParameter.TransmissionRange);
        emitFloat("Subsurface", "TransmissionEdgeGlow", data.subsurfaceScatterParameter.TransmissionEdgeGlow);

        emitBool("Hair", "EnableMultiScatter", data.hairParameter.bEnableMultiScatter);
        emitBool("Hair", "EnableR", data.hairParameter.bEnableR);
        emitBool("Hair", "EnableTT", data.hairParameter.bEnableTT);
        emitBool("Hair", "EnableTRT", data.hairParameter.bEnableTRT);
        emitFloat("Hair", "BackLit", data.hairParameter.backLit);

        emitEnum("Shadow", "Type", data.shadowParameter.shadowType, kShadowTypes);
        emitEnum("Shadow", "Quality", data.shadowParameter.shadowQuality, kShadowQualities);
        emitFloat("Shadow", "DepthBias", data.shadowParameter.shadowDepthBias);
        emitFloat("Shadow", "SlopeDepthBias", data.shadowParameter.shadowSlopeDepthBias);
        emitFloat("Shadow", "MaxSlopeDepthBias", data.shadowParameter.shadowMaxSlopeDepthBias);
        emitFloat("Shadow", "Radius", data.shadowParameter.shadowRadius);
        emitBool("Shadow", "Enable", data.shadowParameter.EnableShadow);
        emitBool("Shadow", "EnableTAA", data.shadowParameter.EnableTAA);

        emitBool("HDR", "Enable", data.hdrParameter.IsUseHDR);
        emitEnum("HDR", "Quality", data.hdrParameter.HDRLevel, kHdrQualities);
        emitEnum("HDR", "Tonemap", data.hdrParameter.tonemapMode, kTonemapModes);
        emitEnum("HDR", "ColorSpace", data.hdrParameter.colorSpace, kColorSpaces);
        emitEnum("HDR", "DisplayMode", data.hdrParameter.displayMode, kDisplayModes);
        emitFloat("HDR", "LocalExposure", data.hdrParameter.localExposure);
        emitBool("HDR", "Shoulder", data.hdrParameter.bShoulder);
        emitFloat("HDR", "SoftGap", data.hdrParameter.SoftGap);
        emitFloat("HDR", "HdrMax", data.hdrParameter.HdrMax);
        emitFloat("HDR", "Exposure", data.hdrParameter.LpmExposure);
        emitFloat("HDR", "Contrast", data.hdrParameter.Contrast);
        emitFloat("HDR", "ShoulderContrast", data.hdrParameter.ShoulderContrast);
        emitVector("HDR", "Saturation", data.hdrParameter.Saturation);
        emitVector("HDR", "Crosstalk", data.hdrParameter.Crosstalk);

        emitBool("AO", "Enable", data.aoParameter.IsEnableAO);
        emitFloat("AO", "Radius", data.aoParameter.Radius);
        emitFloat("AO", "FadeRadius", data.aoParameter.FadeRadius);
        emitFloat("AO", "FadeDistance", data.aoParameter.FadeDistance);
        emitFloat("AO", "Bias", data.aoParameter.Bias);
        emitFloat("AO", "IntensityMul", data.aoParameter.IntensityMul);
        emitFloat("AO", "IntensityPow", data.aoParameter.IntensityPow);
        emitBool("AO", "Lerp", data.aoParameter.IsLerpAO);
        emitFloat("AO", "TAALerpFactor", data.aoParameter.TAALerpFactor);
        emitBool("AO", "Blur", data.aoParameter.IsBlur);
        emitInt("AO", "BlurCount", data.aoParameter.BlurCount);
        emitEnum("AO", "BlurQuality", data.aoParameter.BlurQuality, kAoBlurQualities);
        emitInt("AO", "BlurIntensity", data.aoParameter.BlurIntensity);
        emitFloat("AO", "Sharpness", data.aoParameter.Sharpness);
        emitEnum("AO", "DebugTarget", data.aoParameter.debugTarget, kAoDebugTargets);
        emitFloat("AO", "HIZMipFactor", data.aoParameter.HIZMipFactor);
        emitInt("AO", "HIZMipmap", data.aoParameter.HIZMipmap);
        emitBool("AO", "TAA", data.aoParameter.IsTAA);
        emitFloat("AO", "ImportanceIntensity", data.aoParameter.importanceIntensity);
        emitFloat("AO", "HIZRadius", data.aoParameter.HIZRadius);

        emitBool("GI", "EnableLine", data.GIParameter.enableLine);
        emitBool("GI", "HideInactiveProbe", data.GIParameter.bHideInactiveProbe);
        emitBool("GI", "TextureVisualization", data.GIParameter.bTextureVisualization);
        emitFloat("GI", "LineWidth", data.GIParameter.lineWidth);
        emitFloat("GI", "NormalBias", data.GIParameter.normalBias);
        emitFloat("GI", "ViewBias", data.GIParameter.viewBias);
        emitFloat("GI", "BlendWeight", data.GIParameter.blendWeight);
        emitFloat("GI", "Gamma", data.GIParameter.gamma);
        emitFloat("GI", "ProbeIrradianceThreshold", data.GIParameter.probeIrradianceThreshold);
        emitFloat("GI", "ProbeBrightnessThreshold", data.GIParameter.probeBrightnessThreshold);
        emitVector("GI", "ProbeGroupOrigin", data.GIParameter.probeGroupOrigin);

        emitEnum("Debug", "Mode", data.debugMode, kDebugModes);
        emitInt("Debug", "MipmapLevel", data.mipmapLevel);
        emitInt("Debug", "InstanceID", data.instanceID);
        emitBool("Debug", "EnableHIZ", data.EnableHIZ);

        emitBool("Bloom", "Enable", data.bloomParameter.enable);
        emitFloat("Bloom", "Radius", data.bloomParameter.radius);
        emitFloat("Bloom", "Intensity", data.bloomParameter.intensity);
        emitInt("Bloom", "Mipmap", data.bloomParameter.mipmap);
        emitEnum("Bloom", "DebugDirection", data.bloomParameter.debugMode, kBloomDirections);

        emitBool("TAA", "Enable", data.taaParameter.Enable);
        emitEnum("TAA", "Jitter", data.taaParameter.jitterType, kJitterTypes);
        emitFloat("TAA", "JitterIntensity", data.taaParameter.jitterIntensity);
        emitFloat("TAA", "StaticWeight", data.taaParameter.staticWeight);
        emitFloat("TAA", "DynamicWeight", data.taaParameter.dynamicWeight);
        emitFloat("TAA", "MaxWeight", data.taaParameter.maxWeight);
        emitFloat("TAA", "SampleRate", data.taaParameter.sampleRate);

        emitBool("Sharpen", "Enable", data.sharpenParameter.enable);
        emitFloat("Sharpen", "Amount", data.sharpenParameter.sharpen);

        emitFloat("SSSR", "RoughnessThreshold", data.sssrParameter.roughnessThreshold);
        emitInt("SSSR", "SamplesPerQuad", static_cast<int>(data.sssrParameter.samplesPerQuad));

        const std::filesystem::path saved = ExeDirectory() / "Saved" / "Config" / "Windows" / "Engine.ini";
        std::error_code ec;
        std::filesystem::create_directories(saved.parent_path(), ec);
        if (ec)
        {
            ElysiaHelper::Log::Error("Config: cannot create %s", saved.parent_path().string().c_str());
            return;
        }

        const std::filesystem::path temp = saved.wstring() + L".tmp";
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            if (!file.is_open())
            {
                ElysiaHelper::Log::Error("Config: cannot write %s", temp.string().c_str());
                return;
            }
            file << out.str();
        }

        std::filesystem::rename(temp, saved, ec);
        if (ec)
        {
            std::filesystem::copy_file(temp, saved, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(temp, ec);
        }
    }
}
