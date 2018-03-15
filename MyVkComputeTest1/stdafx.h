// stdafx.h : 標準のシステム インクルード ファイルのインクルード ファイル、または
// 参照回数が多く、かつあまり変更されない、プロジェクト専用のインクルード ファイル
// を記述します。
//

#pragma once

#define WIN32_LEAN_AND_MEAN             // Windows ヘッダーから使用されていない部分を除外します。

#define NOMINMAX // 邪悪なマクロを封印。

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

// 古い SDK (1.0.65.1 以前) ではシステム環境変数 %VK_SDK_PATH% が定義されるが、新しい SDK (1.0.68.0 以降) では %VULKAN_SDK% に変更されていることに注意。
// なぜ変えたし……

#include <vulkan/vulkan.h>

// SDK 1.0 ラッパーの設計ミスの暫定回避策。詳細は実体の定義部を参照。
namespace vk
{
	extern PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
	extern PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
	extern PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
}

// 新しい Vulkan SDK 1.1 の vulkan.hpp では一部のクラスで free() というメンバー関数が定義されているが、
// CRT デバッグ レイヤーの free() マクロが悪影響を与えるので、一時的に無効化する必要がある。
#if (VK_HEADER_VERSION >= 70)
#pragma push_macro("free")
#undef free
#include <vulkan/vulkan.hpp>
#pragma pop_macro("free")
#else
#include <vulkan/vulkan.hpp>
#endif

// SDK 1.0.68.0 の vulkan.hpp には、Device::allocateDescriptorSetsUnique(), Device::allocateCommandBuffersUnique() において std::vector まわりのバグあり。

// SDK 1.1.70.1 の vulkan.hpp には ObjectDestroy まわりのバグあり。
// https://github.com/KhronosGroup/Vulkan-Hpp/issues/189

// そもそもコンパイルが通らないとかどういうことなのか……
// 公式の SDK に含まれるラッパーにコンパイルの通らないバグがあるとか、無責任にもほどがある。
// さてはテストしてねーなオメー。
// かなり不安になってきた。まともにテストされてない半端なライブラリは安心して使えない。vulkan.hpp は使わないほうが無難かもしれない。
// 低レベルなデバッグにボランティアで協力するなど御免こうむる。
// どうしても C++ で RAII を利用したければ、vulkan.hpp は使わず、自前でカスタム デリーターを定義して std::unique_ptr を使ったほうが遥かにマシな気がする。
// 暗黒の OpenGL 時代でもリソース寿命管理に関しては似たような苦労をしていたが、Vulkan はオブジェクトの種類が半端なく多いので、OpenGL 以上に大変になりそう。
// Direct3D は COM のスマートポインタを一様に使えるので、リソース寿命管理に関してはほとんど苦労することもないのだが……
// C API は C++ よりも移植性や相互運用性に優れているが、開発効率が悪いのがネック。
