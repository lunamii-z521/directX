#pragma once

#include <iostream>
#include <vector>
#include <string>


// Request Win32 native handle support from GLFW
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h> // Microsoft::WRL::ComPtr

using namespace DirectX;
using Microsoft::WRL::ComPtr;