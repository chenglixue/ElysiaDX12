#pragma once
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
#include "DXTex/DirectXTex.h"
#include "D3dx12.h"
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"
#include <dxgidebug.h>
#include <numeric>
#include <pix3.h>
#include <unordered_set>
#include <set>
#include "DirectXMath.h"
#include <random>
#include <iostream>
#include <sstream>
#include <locale>
#include <codecvt>

#include "SimpleMath/SimpleMath.h"
#include "Helper.h"
#include "Math.h"
#include "Definition.h"
#include "dxcapi.h"
#include <atlbase.h>        // Common COM helpers.

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

using namespace DirectX;
using namespace ElysiaHelper;
using namespace DirectX::SimpleMath;