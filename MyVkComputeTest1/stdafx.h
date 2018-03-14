// stdafx.h : 標準のシステム インクルード ファイルのインクルード ファイル、または
// 参照回数が多く、かつあまり変更されない、プロジェクト専用のインクルード ファイル
// を記述します。
//

#pragma once

#define WIN32_LEAN_AND_MEAN             // Windows ヘッダーから使用されていない部分を除外します。

#include "targetver.h"

#ifndef NDEBUG
#define _CRTDBG_MAP_ALLOC
#endif
#include <cstdlib>
#include <crtdbg.h>
#include <cstdio>
#include <cstdint>
#include <typeinfo>
#include <iostream>
#include <bitset>
#include <cassert>
#include <tchar.h>
#include <conio.h>
#include <atlbase.h>
#include <atlpath.h>

// TODO: プログラムに必要な追加ヘッダーをここで参照してください

#include <vulkan/vulkan.h>

namespace vk
{
	extern PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
	extern PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
	extern PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
}

#include <vulkan/vulkan.hpp>
