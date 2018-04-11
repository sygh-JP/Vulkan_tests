// MyVkComputeTest1.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

#ifdef _WIN64
#pragma comment(lib, "Lib\\vulkan-1.lib")
//#pragma comment(lib, "Lib\\VkLayer_core_validation.lib")
#else
#pragma comment(lib, "Lib32\\vulkan-1.lib")
//#pragma comment(lib, "Lib32\\VkLayer_core_validation.lib")
#endif

namespace MyUtils
{
	template<typename T> void LoadBinaryFromFileImpl(LPCWSTR pFilePath, std::vector<T>& outBuffer)
	{
		outBuffer.clear();

		struct _stat64 fileStats = {};
		const auto getFileStatFunc = _wstat64;

		if (getFileStatFunc(pFilePath, &fileStats) != 0 || fileStats.st_size < 0)
		{
			throw std::exception("Cannot get the file stats for the file!!");
		}

		if (fileStats.st_size % sizeof(T) != 0)
		{
			throw std::exception("The file size is not a multiple of the expected size of element!!");
		}

		const auto fileSizeInBytes = static_cast<uint64_t>(fileStats.st_size);

		if (sizeof(size_t) < 8 && (std::numeric_limits<size_t>::max)() < fileSizeInBytes)
		{
			throw std::exception("The file size is over the capacity on this platform!!");
		}

		if (fileStats.st_size == 0)
		{
			return;
		}

		const auto numElementsInFile = static_cast<size_t>(fileStats.st_size / sizeof(T));

		outBuffer.resize(numElementsInFile);

		FILE* pFile = nullptr;
		const auto retCode = _wfopen_s(&pFile, pFilePath, L"rb");
		if (retCode != 0 || pFile == nullptr)
		{
			throw std::exception("Cannot open the file!!");
		}
		fread_s(&outBuffer[0], numElementsInFile * sizeof(T), sizeof(T), numElementsInFile, pFile);
		fclose(pFile);
		pFile = nullptr;
	}
}

namespace
{
	// SPIR-V 命令セットの最小単位は4バイトらしい。バイナリのサイズが4の倍数になっていないとおかしい。
	void LoadSpirvFromFile(LPCWSTR pFileName, std::vector<uint32_t>& outSpirv)
	{
		ATL::CPathW path;
		::GetModuleFileNameW(nullptr, path.m_strPath.GetBuffer(MAX_PATH), MAX_PATH);
		path.m_strPath.ReleaseBuffer();
		path.RemoveFileSpec();
		path.Append(L"VkShaders");
		path.Append(pFileName);
		try
		{
			wprintf(L"Now loading SPIR-V from file... <%s>\n", path.m_strPath.GetString());
			MyUtils::LoadBinaryFromFileImpl(path.m_strPath.GetString(), outSpirv);
		}
		catch (...)
		{
			printf("Failed to load SPIR-V from file!!\n");
			throw;
		}
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL MyDebugCallbackFunc(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object,
		size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
	{
		CStringA strMsg("Vulkan DC: ");
		const auto f = static_cast<vk::DebugReportFlagBitsEXT>(flags);
		strMsg.AppendFormat("[%s] (%d) (%s) %s", vk::to_string(f).c_str(), messageCode, pLayerPrefix, pMessage);
#if 0
		strMsg += "\n";
		OutputDebugStringA(strMsg);
#else
		puts(strMsg);
#endif
		return VK_FALSE;
	}
}

// コンパイル時に実体を解決できない（リンク ライブラリに含まれていない）拡張関数をそのまま利用すると、リンク時にエラーが発生する。
// しかし、SDK 1.0.65.1 付属の vulkan.hpp のラッパーでは、あろうことか拡張関数がそのまま使用されている。
// トリッキーな暫定回避策だが、まずラッパーの名前空間に無理やり同名の関数シンボルを宣言＆定義し、
// 実行時に拡張関数を使う前にバインドする。
// グローバル名前空間のシンボルよりもラッパー名前空間のシンボルが優先されることを利用して、ラッパーが参照する実体を差し替える。
// ただし実行時に取得した関数エントリポイントは、Vulkan Instance/Device ごとに存在すること、また Vulkan Instance/Device が消滅したタイミングで無効になることを忘れないように。
// 拡張関数に関しては、SDK 1.0.65.1 時点ではラッパーの実装がダメすぎる。
// 新しいバージョンの SDK に付属する vulkan.hpp では、拡張関数のポインタ群を管理するための vk::DispatchLoaderDynamic クラスが実装されている模様。
// https://github.com/KhronosGroup/Vulkan-Hpp
namespace vk
{
	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
	PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
	PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
}

// Kepler 世代 (CC 3.0) で導入された、CUDA の Warp シャッフル命令 (SIMD) を使えば、共有メモリを使わずとも高速な並列リダクションを実装することが可能になる。
// 汎用的な GPGPU だけでなく、グラフィックスのポストエフェクト目的でも有用。
// https://devblogs.nvidia.com/faster-parallel-reductions-kepler/
// NVIDIA は CUDA の Warp シャッフル命令を HLSL/GLSL で使えるようにする拡張を提供しているが、当然ほかのベンダーでは保証されない。
// OpenGL では良くも悪くも GLSL シェーダーの実行時コンパイルが主流なので、柔軟なベンダー拡張が可能となっている。
// GL_NV_gpu_shader5, GL_NV_shader_thread_group, GL_NV_shader_thread_shuffle も参照。
// https://developer.nvidia.com/reading-between-threads-shader-intrinsics
// AMD も GL_AMD_shader_ballot を提供している。
// https://www.khronos.org/registry/OpenGL/extensions/AMD/AMD_shader_ballot.txt
// 
// できれば標準化された DirectX Shader Model 6.0 の Wave 命令か、Vulkan/OpenGL 公式の Sub-group 拡張命令 (GL_ARB_shader_ballot) を使うべき。
// http://32ipi028l5q82yhj72224m8j.wpengine.netdna-cdn.com/wp-content/uploads/2017/07/GDC2017-Wave-Programming-D3D12-Vulkan.pdf
// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shader_ballot.txt
// ただし、glslangValidator の HLSL モードは、まだ SM 6.0 の機能をフルサポートしていないらしい。
// https://github.com/KhronosGroup/glslang/wiki/HLSL-FAQ
// 
// Vulkan 1.1 では、ARB 拡張よりも高機能な GL_KHR_shader_subgroup 拡張群が追加された。
// Vulkan 1.1 ではまた、実行時にサブグループのデバイス固有仕様に関する情報を取得できるようになった。1.0 時点では同等の拡張機能すらなかった。
// https://www.khronos.org/blog/vulkan-subgroup-tutorial
// なお、glslangValidator は、Vulkan SDK 1.1.70 に含まれるバージョンで SM 6.0 の Wave 命令をサポートするようになったとのこと。

int main()
{
	// CRT のデバッグ レイヤーも有効にしておく。
#ifndef NDEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// 参考：
	// http://www.duskborn.com/a-simple-vulkan-compute-example/
	// https://gist.github.com/sheredom/523f02bbad2ae397d7ed255f3f3b5a7f
	// GLSL/HLSL を使わずに、生の SPIR-V 命令をソースコード中に直接記述している変態的なサンプル。
	// 
	// GLFW を使った Vulkan のサンプルはよく見かけるが、内部が隠ぺいされているため、Vulkan の本当の初期化処理の入門には向いていない。
	// C++ バインディングを使って省力化しつつ、できるかぎりローレベルな初期化処理を明示的に書いてみる。
	// リソースの寿命管理を簡素化するために、std::unique_ptr に相当するラッパーも用意されている。
	// https://github.com/KhronosGroup/Vulkan-Hpp
	// 
	// パラメータの設定をミスすると、GeForce 388.71 ドライバーではアクセス違反 0xC0000005 が発生することがある。
	// その際、Visual Studio の出力ウィンドウには原因特定につながるエラーメッセージが表示されるが、もしエンドユーザー環境で発生したらお手上げ。
	// Vulkan SDK 1.0.65.1 付属の C++ ラッパーレイヤーに問題があるのか、それともドライバーの実装側がロバストでないのかは不明。
	// 
	// Vulkan では OpenGL における WGL, GLX, AGL のようなプラットフォーム独自の API レイヤーは存在せず、すべてエクステンションを用いる方式になっている。
	// どちらかというと OpenCL に近い設計。ラッパーも cl.hpp に近い。
	// もし Vulkan のコンピュート機能だけを使う場合、OS のウィンドウ機構（サーフェイスおよびスワップチェーン）にアクセスする必要がないため、
	// GLFW を使わずともプラットフォーム非依存のコードを書けるはず。
	// ただし OpenCL や CUDA とは違い、Vulkan のコンピュートは GL_ARB_compute_variable_group_size をサポートしないなど、柔軟性・汎用性・記述性・プログラマビリティには欠ける。
	// 初期化の難易度も尋常ではない。

	try
	{
		vk::ApplicationInfo appInfo;
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;
		appInfo.pApplicationName = "My Vulkan Compute Test #1";

		// Direct3D ではデバッグ レイヤーを有効にすると、終了時に D3D オブジェクトのリークがあれば報告されるが、
		// VK_EXT_debug_report を有効にすると、Vulkan オブジェクトのリークもチェックされるようになるのか？
		const char* const desiredExtensionNames[] =
		{
#if 0
			//"VK_KHR_surface",
			//"VK_KHR_win32_surface",
			"VK_EXT_debug_report",
#else
			//VK_KHR_SURFACE_EXTENSION_NAME,
			//VK_KHR_WIN32_SURFACE_EXTENSION_NAME, // VK_USE_PLATFORM_WIN32_KHR を定義してから vulkan.h をインクルードする。
			VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
		};
		const char* const layerNames[] =
		{
			"VK_LAYER_LUNARG_standard_validation",
		};

		vk::InstanceCreateInfo instanceCInfo;
		instanceCInfo.pApplicationInfo = &appInfo;
		instanceCInfo.ppEnabledExtensionNames = desiredExtensionNames;
		instanceCInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(desiredExtensionNames));
		instanceCInfo.ppEnabledLayerNames = layerNames;
		instanceCInfo.enabledLayerCount = static_cast<uint32_t>(std::size(layerNames));

		// Vulkan Instance は DXGI ファクトリのようなものらしい。
		auto instance = vk::createInstanceUnique(instanceCInfo);
		printf("Succeeded in creaing Vulkan instance.\n");

		vk::vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(instance->getProcAddr("vkCreateDebugReportCallbackEXT"));
		vk::vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(instance->getProcAddr("vkDestroyDebugReportCallbackEXT"));
		vk::vkDebugReportMessageEXT = reinterpret_cast<PFN_vkDebugReportMessageEXT>(instance->getProcAddr("vkDebugReportMessageEXT"));

		vk::DebugReportCallbackCreateInfoEXT debugReportCallbackCInfo;
		debugReportCallbackCInfo.flags = vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::ePerformanceWarning;
		debugReportCallbackCInfo.pfnCallback = MyDebugCallbackFunc;

		auto debugReportCallback = instance->createDebugReportCallbackEXTUnique(debugReportCallbackCInfo);

		auto phDevices = instance->enumeratePhysicalDevices();

		for (auto& phDevice : phDevices)
		{
			puts("");

			auto props = phDevice.getProperties();
			printf("DeviceName = %s\n", props.deviceName);

			auto qfPropsArray = phDevice.getQueueFamilyProperties();
			const auto queueFamilyCount = static_cast<uint32_t>(qfPropsArray.size());
			printf("QueueFamilyCount = %u\n", queueFamilyCount);
			// 言語機能で nullable が欲しくなる場面。
			uint32_t queueFamilyIndex = 0;
			const auto searchQFI = [&](auto mask) {
				bool found = false;
				for (uint32_t i = 0; i < queueFamilyCount; ++i)
				{
					const auto& qfProps = qfPropsArray[i];
					// Vulkan C++ ラッパーではきちんと C++11 の enum class が使われていて好印象。文字列化のヘルパー関数も用意されている。
					// C# だと enum の文字列化は簡単なのだが……
					//std::cout << "QueueFlags = " << std::bitset<4>(static_cast<uint32_t>(qfProps.queueFlags)) << std::endl;
					std::cout << "QueueFlags = " << vk::to_string(qfProps.queueFlags) << std::endl;

					if (!found && (qfProps.queueFlags & mask))
					{
						queueFamilyIndex = i;
						found = true;
					}
				}
				return found;
			};
			// NVIDIA GeForce GTX 770 の場合、Queue Family Index は2つある模様。2種類のキューがあるということ？
			// 1個目はすべてのフラグを持っているが、2個目は Transfer のみらしい。
			// 今回は Graphics は使わないので、Compute だけチェックする。
			if (!searchQFI(vk::QueueFlagBits::eCompute))
			{
				printf("Failed to find proper queue!!\n");
				continue;
			}
			printf("First QueueFamilyIndex with Compute = %u\n", queueFamilyIndex);

			auto deProps = phDevice.enumerateDeviceExtensionProperties();
			printf("Extensions:\n");
			for (const auto& prop : deProps)
			{
				printf("%s\n", prop.extensionName);
			}

			const float queuePriorities[1] = { 0.0 };

			vk::DeviceQueueCreateInfo deviceQueueCInfo;
			deviceQueueCInfo.queueCount = 1;
			deviceQueueCInfo.pQueuePriorities = queuePriorities;
			deviceQueueCInfo.queueFamilyIndex = queueFamilyIndex;

			const char* const desiredDevExtNames[] =
			{
#if 0
				"VK_EXT_shader_subgroup_ballot",
				//"VK_EXT_shader_subgroup_vote",
#else
				VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME,
				//VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME,
#endif
			};
			vk::DeviceCreateInfo deviceCInfo;
			deviceCInfo.queueCreateInfoCount = 1;
			deviceCInfo.pQueueCreateInfos = &deviceQueueCInfo;
			deviceCInfo.ppEnabledExtensionNames = desiredDevExtNames;
			deviceCInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(desiredDevExtNames));

			auto device = phDevice.createDeviceUnique(deviceCInfo);
			printf("Succeeded in creating Vulkan device.\n");

			//auto deviceQueue = device.getQueue(queueFamilyIndex, 0);

			auto memProps = phDevice.getMemoryProperties();

			uint32_t memoryTypeIndex = 0;
			const auto searchMTI = [&](auto mask, vk::DeviceSize memSizeInBytes) {
				assert(memProps.memoryTypeCount < std::size(memProps.memoryTypes));
				bool found = false;
				for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
				{
					const auto memType = memProps.memoryTypes[i];
					//std::cout << "MemTypeFlags = " << std::bitset<5>(static_cast<uint32_t>(memType.propertyFlags)) << std::endl;
					std::cout << "MemTypeFlags = " << vk::to_string(memType.propertyFlags) << std::endl;
					assert(memType.heapIndex < std::size(memProps.memoryHeaps));
					if (!found && (memType.propertyFlags & mask) && memSizeInBytes < memProps.memoryHeaps[memType.heapIndex].size)
					{
						memoryTypeIndex = i;
						found = true;
					}
				}
				return found;
			};
			using TElem = float;
			const uint32_t NumElements = 256 * 256;
			const uint32_t OneBufferSizeInBytes = sizeof(TElem) * NumElements;
			const uint32_t RequiredMemSizeInBytes = 2 * OneBufferSizeInBytes;
			if (!searchMTI(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, RequiredMemSizeInBytes))
			{
				printf("Failed to find proper memory!!\n");
				continue;
			}
			printf("First MemoryTypeIndex with HostVisible and HostCoherent = %u\n", memoryTypeIndex);

			vk::MemoryAllocateInfo memoryAInfo;
			memoryAInfo.allocationSize = RequiredMemSizeInBytes;
			memoryAInfo.memoryTypeIndex = memoryTypeIndex;
			auto deviceMem = device->allocateMemoryUnique(memoryAInfo);
			printf("Succeeded in allocating device memory.\n");

			// 確保したメモリは既定で最初からゼロクリアされている模様。
			{
				auto* hostMemPtr = static_cast<TElem*>(device->mapMemory(deviceMem.get(), 0, RequiredMemSizeInBytes));
				assert(hostMemPtr);
				for (uint32_t i = 0; i < NumElements; ++i)
				{
					hostMemPtr[i] = static_cast<TElem>(i);
				}
				device->unmapMemory(deviceMem.get());
				hostMemPtr = nullptr;
			}

			// Vulkan のバッファはメモリを直接管理するオブジェクトではないらしい。論理的な抽象オブジェクト。
			vk::BufferCreateInfo bufferCInfo;
			bufferCInfo.size = OneBufferSizeInBytes;
			bufferCInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
			bufferCInfo.sharingMode = vk::SharingMode::eExclusive;
			bufferCInfo.queueFamilyIndexCount = 1;
			bufferCInfo.pQueueFamilyIndices = &queueFamilyIndex;

			auto bufferIn = device->createBufferUnique(bufferCInfo);
			printf("Succeeded in creating buffer.\n");
			auto bufferOut = device->createBufferUnique(bufferCInfo);
			printf("Succeeded in creating buffer.\n");

			// vkGetBufferMemoryRequirements() を呼ばないで vkBindBufferMemory() を呼び出すと Validation Layer から警告が出る。
			auto bufMemReqIn = device->getBufferMemoryRequirements(bufferIn.get());
			auto bufMemReqOut = device->getBufferMemoryRequirements(bufferOut.get());
			assert(bufMemReqIn.size == OneBufferSizeInBytes);
			assert(bufMemReqOut.size == OneBufferSizeInBytes);

			device->bindBufferMemory(bufferIn.get(), deviceMem.get(), 0);
			device->bindBufferMemory(bufferOut.get(), deviceMem.get(), OneBufferSizeInBytes);

			// 各要素の初期化に uniform initialization を使えば型名を省略できるが、コード補完のヒントを見たいので、あえてコンストラクタ呼び出しで書く。
			vk::DescriptorSetLayoutBinding descriptorSetLayoutBindings[2] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute),
			};

			vk::DescriptorSetLayoutCreateInfo descSetLayoutCInfo;
			descSetLayoutCInfo.bindingCount = 2;
			descSetLayoutCInfo.pBindings = descriptorSetLayoutBindings;

			auto descSetLayout = device->createDescriptorSetLayoutUnique(descSetLayoutCInfo);
			printf("Succeeded in creating descripter set layout.\n");

			vk::PipelineLayoutCreateInfo pipelineLayoutCInfo;
			pipelineLayoutCInfo.setLayoutCount = 1;
			pipelineLayoutCInfo.pSetLayouts = &descSetLayout.get();

			auto pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutCInfo);
			printf("Succeeded in creating pipeline layout.\n");

			std::vector<uint32_t> spirvComputeShader;
			LoadSpirvFromFile(L"test1.comp.spv", spirvComputeShader);

			vk::ShaderModuleCreateInfo shaderModuleCInfo;
			shaderModuleCInfo.codeSize = spirvComputeShader.size() * sizeof(uint32_t);
			shaderModuleCInfo.pCode = spirvComputeShader.data();

			// Vulkan SDK 1.0.65.1 では、gl_SubGroupSizeARB や gl_SubGroupInvocationARB を使うシェーダーコードを
			// vkCreateShaderModule() に渡すと、Validation Layer がエラーを吐く模様。
			// ただし vkCreateShaderModule() 自体は成功コードを返す模様。シェーダーも普通に動作する模様。
			// "SPIR-V module not valid: Operand 3 of Decorate requires one of these capabilities: Kernel"
			// https://github.com/KhronosGroup/glslang/issues/1200

			auto moduleComputeShader = device->createShaderModuleUnique(shaderModuleCInfo);
			printf("Succeeded in creating shader module.\n");

			vk::PipelineShaderStageCreateInfo computePipelineShaderStageCInfo;
			computePipelineShaderStageCInfo.stage = vk::ShaderStageFlagBits::eCompute;
			computePipelineShaderStageCInfo.module = moduleComputeShader.get();
			computePipelineShaderStageCInfo.pName = "main"; // シェーダーの関数エントリポイント名に合わせる必要がある。

			vk::ComputePipelineCreateInfo computePipelienCInfo;
			computePipelienCInfo.layout = pipelineLayout.get();
			computePipelienCInfo.stage = computePipelineShaderStageCInfo;

			auto computePipeline = device->createComputePipelineUnique(vk::PipelineCache(), computePipelienCInfo);
			printf("Succeeded in creating compute pipeline.\n");

			vk::DescriptorPoolSize descPoolSize;
			descPoolSize.type = vk::DescriptorType::eStorageBuffer;
			descPoolSize.descriptorCount = 2;

			vk::DescriptorPoolCreateInfo descPoolCInfo;
			descPoolCInfo.maxSets = 1;
			descPoolCInfo.poolSizeCount = 1;
			descPoolCInfo.pPoolSizes = &descPoolSize;
			descPoolCInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
			// VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT 相当のフラグを立てておかないと、
			// リソース解放時に vkFreeDescriptorSets() の呼び出しが不正だとして Validation Layer がエラーを報告する。

			auto descPool = device->createDescriptorPoolUnique(descPoolCInfo);
			printf("Succeeded in creating descriptor pool.\n");

			vk::DescriptorSetAllocateInfo descSetAInfo;
			descSetAInfo.descriptorPool = descPool.get();
			descSetAInfo.descriptorSetCount = 1;
			descSetAInfo.pSetLayouts = &descSetLayout.get();

			// vkAllocateDescriptorSets() は複数の DescriptorSet をまとめて割り当てる設計になっている。
			// C++ ラッパーでは、std::vector<vk::DescriptorSet> を返す関数が用意されている。
			// vk::DescriptorSet へのポインタを渡す関数もあり、単一の DescriptorSet を割り当てることもできるが、
			// エラー発生時は例外スローではなく戻り値でエラーコードを返すようになっている。
			// vkAllocateCommandBuffers() も同様。
			auto descSetArray = device->allocateDescriptorSetsUnique(descSetAInfo);
			printf("Succeeded in allocating descriptor sets.\n");
			assert(descSetArray.size() == 1);
			auto& descSet = descSetArray[0];

			vk::DescriptorBufferInfo descBufferInfoIn;
			descBufferInfoIn.buffer = bufferIn.get();
			descBufferInfoIn.range = VK_WHOLE_SIZE;

			vk::DescriptorBufferInfo descBufferInfoOut;
			descBufferInfoOut.buffer = bufferOut.get();
			descBufferInfoOut.range = VK_WHOLE_SIZE;

			vk::WriteDescriptorSet writeDescSets[2] =
			{
				vk::WriteDescriptorSet(descSet.get(), 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &descBufferInfoIn),
				vk::WriteDescriptorSet(descSet.get(), 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &descBufferInfoOut),
			};
			device->updateDescriptorSets(2, writeDescSets, 0, nullptr);

			vk::CommandPoolCreateInfo commandPoolCInfo;
			commandPoolCInfo.queueFamilyIndex = queueFamilyIndex;

			auto commandPool = device->createCommandPoolUnique(commandPoolCInfo);
			printf("Succeeded in creating command pool.\n");

			vk::CommandBufferAllocateInfo commandBufferAInfo;
			commandBufferAInfo.commandBufferCount = 1;
			commandBufferAInfo.commandPool = commandPool.get();
			commandBufferAInfo.level = vk::CommandBufferLevel::ePrimary;

			auto commandBufferArray = device->allocateCommandBuffersUnique(commandBufferAInfo);
			printf("Succeeded in allocating command buffers.\n");
			assert(commandBufferArray.size() == 1);
			auto& commandBuffer = commandBufferArray[0];

			vk::CommandBufferBeginInfo commandBufferBInfo;
			commandBufferBInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

			commandBuffer->begin(commandBufferBInfo);
			commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline.get());
			commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout.get(), 0, 1, &descSet.get(), 0, nullptr);
			//commandBuffer.dispatch(16, 16, 1);
			commandBuffer->dispatch(256, 1, 1);
			commandBuffer->end();

			vk::SubmitInfo submitInfo;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer.get();

			auto deviceQueue = device->getQueue(queueFamilyIndex, 0);

			auto submitResult = deviceQueue.submit(1, &submitInfo, vk::Fence());
			std::cout << "Result of submit = " << vk::to_string(submitResult) << std::endl;
			deviceQueue.waitIdle();

			// 処理結果を確認する。
			{
				auto* hostMemPtr = static_cast<TElem*>(device->mapMemory(deviceMem.get(), 0, RequiredMemSizeInBytes));
				assert(hostMemPtr);
				auto* inMemPtr = hostMemPtr;
				auto* outMemPtr = hostMemPtr + NumElements;
				for (uint32_t i = 0; i < NumElements; ++i)
				{
					const auto inVal = inMemPtr[i];
					const auto outVal = outMemPtr[i];
				}
				device->unmapMemory(deviceMem.get());
				hostMemPtr = nullptr;
			}

			// ひとまず最初のデバイスだけテストして終了。
			break;
		}
	}
	catch (const std::exception& ex)
	{
		printf("Exception=%s, Message=\"%s\"\n", typeid(ex).name(), ex.what());
	}

	puts("Press any...");
	_getch();
	return 0;
}
