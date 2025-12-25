#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <windowsx.h>
#include <wrl.h>
#include <shellapi.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include "dxcapi.h"
#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include "ThirdParty/DXTex/DirectXTex.h"
#include "dxc/inc/d3dx12.h"
#include "ThirdParty/D3D12MemoryAllocator/D3D12MemAlloc.h"
#include <dxgidebug.h>
#include <numeric>
#include <pix3.h>
#include <unordered_set>
#include <set>
#include <map>
#include "DirectXMath.h"
#include <random>
#include <iostream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <fstream>
#include <filesystem>
#include <comdef.h> // For _com_error
#include <locale>
#include <codecvt>
#include <bitset>
#include <functional>
#include "ThirdParty/Metalib.h"
#include <boost/container/stable_vector.hpp>

#include <EASTL/internal/config.h>
#include "include/EASTL/string.h"
#include "include/EASTL/vector.h"
#include "include/EASTL/hash_map.h"
#include "include/EASTL/algorithm.h"
#include "include/EASTL/internal/hashtable.h"
#include "include/EASTL/internal/fixed_pool.h"
#include "include/EASTL/intrusive_list.h"

#include "magic_enum/magic_enum.hpp"
#include "ThirdParty/SimpleMath/SimpleMath.h"
#include <atlbase.h>        // Common COM helpers.

#include <WICTextureLoader.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;



#pragma comment(lib, "Windowscodecs.lib")
#pragma comment(lib, "RuntimeObject.lib")
