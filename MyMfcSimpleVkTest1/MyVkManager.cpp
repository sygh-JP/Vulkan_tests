#include "stdafx.h"
#include "MyVkManager.h"

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

	inline uint64_t CalcAlignedSize(uint64_t x, uint64_t align)
	{
		// x = 0 -> 0
		// x = [align * 0 + 1, align * 1] -> align * 1
		// x = [align * 1 + 1, align * 2] -> align * 2
		// ...
		if (x > 0 && align > 0)
		{
			return ((x - 1) / align + 1) * align;
		}
		else
		{
			return x;
		}
	}
}

namespace
{
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
			ATLTRACE(L"Now loading SPIR-V from file... <%s>\n", path.m_strPath.GetString());
			MyUtils::LoadBinaryFromFileImpl(path.m_strPath.GetString(), outSpirv);
		}
		catch (...)
		{
			ATLTRACE("Failed to load SPIR-V from file!!\n");
			throw;
		}
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL MyDebugCallbackFunc(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object,
		size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
	{
		CStringA strMsg("Vulkan DC: ");
		const auto f = static_cast<vk::DebugReportFlagBitsEXT>(flags);
		strMsg.AppendFormat("[%s] (%d) (%s) %s", vk::to_string(f).c_str(), messageCode, pLayerPrefix, pMessage);
		strMsg += "\n";
		::OutputDebugStringA(strMsg);
		return VK_FALSE;
	}

	auto CreateMyVkShaderModuleUniqueFromFile(const vk::Device& device, LPCWSTR pFileName)
	{
		std::vector<uint32_t> spirvShader;
		LoadSpirvFromFile(pFileName, spirvShader);

		vk::ShaderModuleCreateInfo shaderModuleCInfo;
		shaderModuleCInfo.codeSize = spirvShader.size() * sizeof(uint32_t);
		shaderModuleCInfo.pCode = spirvShader.data();

		return device.createShaderModuleUnique(shaderModuleCInfo);
	}

	auto CreateMyVkPipelineUnique(const vk::Device& device,
		const vk::PipelineLayout& pipelineLayout, const vk::RenderPass& renderPass,
		const vk::ShaderModule& vertexShader, const vk::ShaderModule& pixelShader,
		const vk::Viewport& viewport, const vk::Rect2D& rectScissor,
		const vk::PipelineColorBlendAttachmentState& blendState,
		const vk::VertexInputBindingDescription& viBindDesc, const vk::VertexInputAttributeDescription viAttrDescs[], size_t viAttrDescsCount)
	{
		vk::PipelineShaderStageCreateInfo shaderStageCInfo[2];
		vk::PipelineVertexInputStateCreateInfo viStateCInfo;
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblerStateCInfo;
		vk::PipelineViewportStateCreateInfo viewportStateCInfo;
		vk::PipelineRasterizationStateCreateInfo rasterizerStateCInfo;
		vk::PipelineMultisampleStateCreateInfo msStateCInfo;
		vk::PipelineColorBlendStateCreateInfo blendStateCInfo;
		vk::PipelineDynamicStateCreateInfo dynamicStateCInfo;

		const char* const mainEntryFuncName = "main";

		// シェーダーステージは順番通り並べないといけない？
		// HS -> DS -> VS -> GS -> PS
		// (TCS -> TES -> VS -> GS -> FS)

		shaderStageCInfo[0].stage = vk::ShaderStageFlagBits::eVertex;
		shaderStageCInfo[0].module = vertexShader;
		shaderStageCInfo[0].pName = mainEntryFuncName;

		shaderStageCInfo[1].stage = vk::ShaderStageFlagBits::eFragment;
		shaderStageCInfo[1].module = pixelShader;
		shaderStageCInfo[1].pName = mainEntryFuncName;

		viStateCInfo.vertexBindingDescriptionCount = 1;
		viStateCInfo.pVertexBindingDescriptions = &viBindDesc;
		viStateCInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(viAttrDescsCount);
		viStateCInfo.pVertexAttributeDescriptions = viAttrDescs;

		inputAssemblerStateCInfo.topology = vk::PrimitiveTopology::eTriangleList;

		viewportStateCInfo.viewportCount = 1;
		viewportStateCInfo.pViewports = &viewport;
		viewportStateCInfo.scissorCount = 1;
		viewportStateCInfo.pScissors = &rectScissor;

		rasterizerStateCInfo.depthClampEnable = VK_FALSE;
		rasterizerStateCInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizerStateCInfo.polygonMode = vk::PolygonMode::eFill;
		rasterizerStateCInfo.cullMode = vk::CullModeFlagBits::eNone;
		rasterizerStateCInfo.frontFace = vk::FrontFace::eCounterClockwise;
		rasterizerStateCInfo.depthBiasEnable = VK_FALSE;
		rasterizerStateCInfo.lineWidth = 1.0f;

		msStateCInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
		msStateCInfo.sampleShadingEnable = VK_FALSE;
		msStateCInfo.alphaToCoverageEnable = VK_FALSE;
		msStateCInfo.alphaToOneEnable = VK_FALSE;

		blendStateCInfo.logicOpEnable = VK_FALSE;
		blendStateCInfo.attachmentCount = 1;
		blendStateCInfo.pAttachments = &blendState;

		// ビューポートおよびシザー矩形をレンダリング ループにて動的に変更するために必要らしい。Direct3D 12 では特に指定する必要はない。
		// 動的ステート変更を OFF にするとパフォーマンス上のメリットがあるのかもしれない。ひとまず ON にしておく。
		const vk::DynamicState dynamicStates[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

		dynamicStateCInfo.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
		dynamicStateCInfo.pDynamicStates = dynamicStates;

		vk::GraphicsPipelineCreateInfo gpCInfo;
		gpCInfo.stageCount = static_cast<uint32_t>(std::size(shaderStageCInfo));
		gpCInfo.pStages = shaderStageCInfo;
		gpCInfo.pVertexInputState = &viStateCInfo;
		gpCInfo.pInputAssemblyState = &inputAssemblerStateCInfo;
		gpCInfo.pViewportState = &viewportStateCInfo;
		gpCInfo.pRasterizationState = &rasterizerStateCInfo;
		gpCInfo.pMultisampleState = &msStateCInfo;
		gpCInfo.pColorBlendState = &blendStateCInfo;
		gpCInfo.pDynamicState = &dynamicStateCInfo;
		gpCInfo.layout = pipelineLayout;
		gpCInfo.renderPass = renderPass;
		gpCInfo.subpass = 0;

		return device.createGraphicsPipelineUnique(vk::PipelineCache(), gpCInfo);
	}

	void BeginMyVkCommandBuffer(const vk::CommandBuffer& cmdBuffer, const vk::Framebuffer& fb)
	{
		vk::CommandBufferInheritanceInfo inheritInfo;
		vk::CommandBufferBeginInfo beginInfo;

		inheritInfo.framebuffer = fb;
		beginInfo.pInheritanceInfo = &inheritInfo;

		cmdBuffer.begin(beginInfo);
	}

	void BarrierMyVkResource(const vk::CommandBuffer& cmdBuffer, const vk::Image& image,
		vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask,
		vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
		vk::ImageLayout srcImageLayout, vk::ImageLayout dstImageLayout)
	{
		vk::ImageMemoryBarrier barrier;
		barrier.image = image;
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;
		barrier.oldLayout = srcImageLayout;
		barrier.newLayout = dstImageLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
		cmdBuffer.pipelineBarrier(srcStageMask, dstStageMask, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &barrier);
	}

	void SubmitMyVkCommandQueue(const vk::Queue& queue, const vk::CommandBuffer& cmdBuffer, const vk::Fence& fence)
	{
		const vk::PipelineStageFlags stageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
		vk::SubmitInfo submitInfo;
		submitInfo.pWaitDstStageMask = &stageMask;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer;

		vk::createResultValue(queue.submit(1, &submitInfo, fence), "vkQueueSubmit Failed!!");
	}

	void ResetMyVkFence(const vk::Device& device, const vk::Fence& fence)
	{
		vk::createResultValue(device.resetFences(1, &fence), "vkResetFences Failed!!");
	}

	void WaitForMyVkFence(const vk::Device& device, const vk::Fence& fence, uint64_t timeoutNS)
	{
		vk::createResultValue(device.waitForFences(1, &fence, VK_FALSE, timeoutNS), "vkWaitForFences Failed!!");
	}

	void AcquireMyVkNextImageAndWait(const vk::Device& device, const vk::SwapchainKHR& swapchain, const vk::Fence& fence, uint64_t timeoutNS, uint32_t& nextFrameIndex)
	{
		vk::createResultValue(device.acquireNextImageKHR(swapchain, timeoutNS, vk::Semaphore(), fence, &nextFrameIndex), "vkAcquireNextImageKHR Failed!!");
		WaitForMyVkFence(device, fence, timeoutNS);
		ResetMyVkFence(device, fence);
	}

	void PresentMyVkQueue(const vk::Queue& queue, const vk::SwapchainKHR& swapchain, uint32_t frameIndex)
	{
		vk::PresentInfoKHR presentInfo;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain;
		presentInfo.pImageIndices = &frameIndex;

		vk::createResultValue(queue.presentKHR(presentInfo), "vkQueuePresentKHR Failed!!");
	}

	void BeginMyVkRenderPass(const vk::CommandBuffer& cmdBuffer, const vk::Framebuffer& framebuffer, const vk::RenderPass& renderPass, const vk::Rect2D& renderArea, const vk::ClearColorValue& clearColor)
	{
		const vk::ClearValue clearValue = clearColor;

		vk::RenderPassBeginInfo renderPassBInfo;
		renderPassBInfo.framebuffer = framebuffer;
		renderPassBInfo.renderPass = renderPass;
		renderPassBInfo.renderArea = renderArea;
		renderPassBInfo.clearValueCount = 1;
		renderPassBInfo.pClearValues = &clearValue;

		cmdBuffer.beginRenderPass(renderPassBInfo, vk::SubpassContents::eInline);
	}
}

namespace vk
{
	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
	PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
	PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
}

// 参考：
// https://qiita.com/Pctg-x8/items/6fd3ef76b58d98f26634
// https://github.com/Pctg-x8/vkTest

void MyVkManager::Init(HWND hWnd, UINT width, UINT height)
{
	{
		m_viewport.width = static_cast<float>(width);
		m_viewport.height = static_cast<float>(height);
		m_viewport.maxDepth = 1;

		m_rectScissor.extent.width = width;
		m_rectScissor.extent.height = height;

		m_renderArea.extent.width = width;
		m_renderArea.extent.height = height;
	}

	vk::ApplicationInfo appInfo;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;
	appInfo.pApplicationName = "My Vulkan Simple Test #1";

	const char* const desiredExtensionNames[] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
	};
	// HACK: 通常はデバッグ ビルドでのみ validation layer を使えばよいはず。
	// デバッグ レポート機能のほうはフィールド（エンドユーザー環境）でも有効にしておいたほうがよいかも。
	// デバッグ レポートのほうは、実際にエラーが起きてコールバックされないかぎりオーバーヘッドにはならないはず。
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

	m_vkInstance = vk::createInstanceUnique(instanceCInfo);
	const auto& instance = m_vkInstance;
	ATLTRACE("Succeeded in creaing Vulkan instance.\n");

	vk::vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(instance->getProcAddr("vkCreateDebugReportCallbackEXT"));
	vk::vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(instance->getProcAddr("vkDestroyDebugReportCallbackEXT"));
	vk::vkDebugReportMessageEXT = reinterpret_cast<PFN_vkDebugReportMessageEXT>(instance->getProcAddr("vkDebugReportMessageEXT"));

	vk::DebugReportCallbackCreateInfoEXT debugReportCallbackCInfo;
	debugReportCallbackCInfo.flags = vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::ePerformanceWarning;
	debugReportCallbackCInfo.pfnCallback = MyDebugCallbackFunc;

	m_vkDebugReportCallback = instance->createDebugReportCallbackEXTUnique(debugReportCallbackCInfo);

	auto phDevices = instance->enumeratePhysicalDevices();

	for (auto& phDevice : phDevices)
	{
		const auto props = phDevice.getProperties();
		ATLTRACE("DeviceName = %s\n", props.deviceName);

		const auto qfPropsArray = phDevice.getQueueFamilyProperties();
		const auto queueFamilyCount = static_cast<uint32_t>(qfPropsArray.size());
		ATLTRACE("QueueFamilyCount = %u\n", queueFamilyCount);
		uint32_t queueFamilyIndex = 0;
		const auto searchQFI = [&](auto mask) {
			bool found = false;
			for (uint32_t i = 0; i < queueFamilyCount; ++i)
			{
				const auto& qfProps = qfPropsArray[i];
				ATLTRACE("QueueFlags = %s\n", vk::to_string(qfProps.queueFlags).c_str());

				if (!found && (qfProps.queueFlags & mask))
				{
					queueFamilyIndex = i;
					found = true;
				}
			}
			return found;
		};
		if (!searchQFI(vk::QueueFlagBits::eGraphics))
		{
			ATLTRACE("Failed to find proper queue!!\n");
			continue;
		}
		ATLTRACE("First QueueFamilyIndex with Graphics = %u\n", queueFamilyIndex);

#if 0
		const auto deProps = phDevice.enumerateDeviceExtensionProperties();
		ATLTRACE("Extensions:\n");
		for (const auto& prop : deProps)
		{
			ATLTRACE("%s\n", prop.extensionName);
		}
#endif

		const float queuePriorities[1] = { 0.0 };

		vk::DeviceQueueCreateInfo deviceQueueCInfo;
		deviceQueueCInfo.queueCount = 1;
		deviceQueueCInfo.pQueuePriorities = queuePriorities;
		deviceQueueCInfo.queueFamilyIndex = queueFamilyIndex;

		const char* const desiredDevExtNames[] =
		{
#if 0
			"VK_KHR_swapchain",
#else
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#endif
		};
		vk::DeviceCreateInfo deviceCInfo;
		deviceCInfo.queueCreateInfoCount = 1;
		deviceCInfo.pQueueCreateInfos = &deviceQueueCInfo;
		deviceCInfo.ppEnabledExtensionNames = desiredDevExtNames;
		deviceCInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(desiredDevExtNames));

		m_vkDevice = phDevice.createDeviceUnique(deviceCInfo);

		m_vkDeviceQueue = m_vkDevice->getQueue(queueFamilyIndex, 0);

		const auto memProps = phDevice.getMemoryProperties();

		const auto searchMTI = [&](uint32_t& memoryTypeIndex, auto mask, vk::DeviceSize memSizeInBytes) {
			ATLASSERT(memProps.memoryTypeCount < std::size(memProps.memoryTypes));
			bool found = false;
			for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
			{
				const auto memType = memProps.memoryTypes[i];
				ATLTRACE("MemTypeFlags = %s\n", vk::to_string(memType.propertyFlags).c_str());
				ATLASSERT(memType.heapIndex < std::size(memProps.memoryHeaps));
				if (!found && (memType.propertyFlags & mask) && memSizeInBytes < memProps.memoryHeaps[memType.heapIndex].size)
				{
					memoryTypeIndex = i;
					found = true;
				}
			}
			return found;
		};

		vk::Win32SurfaceCreateInfoKHR surfaceCInfo;
		surfaceCInfo.hinstance = ::GetModuleHandle(nullptr);
		surfaceCInfo.hwnd = hWnd;

		m_vkSurface = instance->createWin32SurfaceKHRUnique(surfaceCInfo);

		const auto supportsSurface = phDevice.getSurfaceSupportKHR(queueFamilyIndex, m_vkSurface.get());
		if (!supportsSurface)
		{
			ATLTRACE("Surface is not supported!!\n");
			continue;
		}

		const auto surfaceCaps = phDevice.getSurfaceCapabilitiesKHR(m_vkSurface.get());

		const auto surfaceFormats = phDevice.getSurfaceFormatsKHR(m_vkSurface.get());
		const auto targetFormat = vk::Format::eB8G8R8A8Unorm;
		bool supportsTargetFormat = false;
		ATLTRACE("Surface Formats:\n");
		for (const auto& surfaceFormat : surfaceFormats)
		{
			ATLTRACE("%s\n", vk::to_string(surfaceFormat.format).c_str());
			if (surfaceFormat.format == targetFormat)
			{
				supportsTargetFormat = true;
			}
		}
		if (!supportsTargetFormat)
		{
			ATLTRACE("Surface does not support %s!!\n", vk::to_string(targetFormat).c_str());
			continue;
		}

		{
			vk::SwapchainCreateInfoKHR swapchainCInfo;
			swapchainCInfo.surface = m_vkSurface.get();
			swapchainCInfo.minImageCount = 2;
			swapchainCInfo.imageFormat = targetFormat;
			swapchainCInfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
			swapchainCInfo.imageExtent.width = width;
			swapchainCInfo.imageExtent.height = height;
			swapchainCInfo.imageArrayLayers = 1;
			swapchainCInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
			swapchainCInfo.imageSharingMode = vk::SharingMode::eExclusive;
			swapchainCInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
			swapchainCInfo.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
			swapchainCInfo.presentMode = vk::PresentModeKHR::eFifo;
			swapchainCInfo.clipped = VK_TRUE;

			m_vkSwapchain = m_vkDevice->createSwapchainKHRUnique(swapchainCInfo);
		}

		m_vkSwapchainImages = m_vkDevice->getSwapchainImagesKHR(m_vkSwapchain.get());

		m_vkImageViews.clear();
		for (const auto& image : m_vkSwapchainImages)
		{
			vk::ImageViewCreateInfo imageViewCInfo;
			imageViewCInfo.image = image;
			imageViewCInfo.viewType = vk::ImageViewType::e2D;
			imageViewCInfo.format = targetFormat;
			imageViewCInfo.components.r = vk::ComponentSwizzle::eR;
			imageViewCInfo.components.g = vk::ComponentSwizzle::eG;
			imageViewCInfo.components.b = vk::ComponentSwizzle::eB;
			imageViewCInfo.components.a = vk::ComponentSwizzle::eA;
			imageViewCInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			imageViewCInfo.subresourceRange.levelCount = 1;
			imageViewCInfo.subresourceRange.layerCount = 1;

			m_vkImageViews.push_back(std::move(m_vkDevice->createImageViewUnique(imageViewCInfo)));
		}

		{
			vk::AttachmentDescription attachmentDesc;
			attachmentDesc.format = targetFormat;
			attachmentDesc.samples = vk::SampleCountFlagBits::e1;
			attachmentDesc.loadOp = vk::AttachmentLoadOp::eClear;
			attachmentDesc.storeOp = vk::AttachmentStoreOp::eStore;
			attachmentDesc.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
			attachmentDesc.finalLayout = vk::ImageLayout::ePresentSrcKHR;
			vk::AttachmentReference attachmentRef;
			attachmentRef.attachment = 0;
			attachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

			vk::SubpassDescription subpass;
			subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &attachmentRef;
			vk::RenderPassCreateInfo renderPassCInfo;
			renderPassCInfo.attachmentCount = 1;
			renderPassCInfo.pAttachments = &attachmentDesc;
			renderPassCInfo.subpassCount = 1;
			renderPassCInfo.pSubpasses = &subpass;

			m_vkRenderPass = m_vkDevice->createRenderPassUnique(renderPassCInfo);
		}

		{
			vk::FramebufferCreateInfo frameBufferCInfo;
			vk::ImageView attachmentViews[1];

			frameBufferCInfo.attachmentCount = 1;
			frameBufferCInfo.renderPass = m_vkRenderPass.get();
			frameBufferCInfo.pAttachments = attachmentViews;
			frameBufferCInfo.width = width;
			frameBufferCInfo.height = height;
			frameBufferCInfo.layers = 1;

			m_vkFrameBuffers.clear();
			for (const auto& view : m_vkImageViews)
			{
				attachmentViews[0] = view.get();

				m_vkFrameBuffers.push_back(std::move(m_vkDevice->createFramebufferUnique(frameBufferCInfo)));
			}
		}

		{
			const float outerRadius = 0.7f;
			const float posX = outerRadius * cos(DirectX::XMConvertToRadians(30));
			const float posY = outerRadius * sin(DirectX::XMConvertToRadians(30));
			const MyVertex triangleVerts[] =
			{
				{ MyVector3F{ 0.0f, +outerRadius, 0.0f }, MyVector4F{ 1.0f, 0.0f, 0.0f, 1.0f } },
				{ MyVector3F{ -posX, -posY, 0.0f }, MyVector4F{ 0.0f, 1.0f, 0.0f, 1.0f } },
				{ MyVector3F{ +posX, -posY, 0.0f }, MyVector4F{ 0.0f, 0.0f, 1.0f, 1.0f } },
			};
			const size_t RequiredMemSizeInBytes = sizeof(triangleVerts);

			// パフォーマンスを重視する場合は eDeviceLocal を使うべきだが、CPU コードでの書き換え処理が面倒になるので、
			// 簡単のため eHostVisible を使う。動的頂点バッファに相当する。

			uint32_t memoryTypeIndex = 0;
			if (!searchMTI(memoryTypeIndex, vk::MemoryPropertyFlagBits::eHostVisible, RequiredMemSizeInBytes))
			{
				ATLTRACE("Failed to find proper memory!!\n");
				continue;
			}
			ATLTRACE("First MemoryTypeIndex with HostVisible = %u\n", memoryTypeIndex);

			vk::BufferCreateInfo bufferCInfo;
			bufferCInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
			bufferCInfo.size = RequiredMemSizeInBytes;
			bufferCInfo.sharingMode = vk::SharingMode::eExclusive;
			//bufferCInfo.queueFamilyIndexCount = 1;
			//bufferCInfo.pQueueFamilyIndices = &queueFamilyIndex;

			m_vkVertexBuffer = m_vkDevice->createBufferUnique(bufferCInfo);

			auto bufMemReq = m_vkDevice->getBufferMemoryRequirements(m_vkVertexBuffer.get());

			// Vulkan のバッファは256バイト境界でアライメントされていなければならない模様（数値はハードウェア依存かもしれない）。
			// アライメント情報は VkMemoryRequirements で確認できるらしい。
			// 中途半端なサイズを指定しても vkAllocateMemory() でメモリ確保はできるようだが、vkBindBufferMemory() 呼び出しの際に validation layer からエラーが報告される。

			ATLASSERT(bufMemReq.alignment > 0);
			const auto alignedSizeInBytes = MyUtils::CalcAlignedSize(RequiredMemSizeInBytes, bufMemReq.alignment);
			ATLASSERT(bufMemReq.size == alignedSizeInBytes);

			vk::MemoryAllocateInfo memoryAInfo;
			memoryAInfo.allocationSize = bufMemReq.size;
			memoryAInfo.memoryTypeIndex = memoryTypeIndex;

			m_vkVertexBufferMemory = m_vkDevice->allocateMemoryUnique(memoryAInfo);

			{
				auto* hostMemPtr = m_vkDevice->mapMemory(m_vkVertexBufferMemory.get(), 0, RequiredMemSizeInBytes);
				ATLASSERT(hostMemPtr);
				memcpy(hostMemPtr, triangleVerts, RequiredMemSizeInBytes);
				m_vkDevice->unmapMemory(m_vkVertexBufferMemory.get());
				hostMemPtr = nullptr;
			}

			m_vkDevice->bindBufferMemory(m_vkVertexBuffer.get(), m_vkVertexBufferMemory.get(), 0);
		}

		{
			const size_t RequiredMemSizeInBytes = sizeof(MyConstBuffer);

			uint32_t memoryTypeIndex = 0;
			if (!searchMTI(memoryTypeIndex, vk::MemoryPropertyFlagBits::eHostVisible, RequiredMemSizeInBytes))
			{
				ATLTRACE("Failed to find proper memory!!\n");
				continue;
			}
			ATLTRACE("First MemoryTypeIndex with HostVisible = %u\n", memoryTypeIndex);

			vk::BufferCreateInfo bufferCInfo;
			bufferCInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
			bufferCInfo.size = RequiredMemSizeInBytes;
			bufferCInfo.sharingMode = vk::SharingMode::eExclusive;

			m_vkConstBuffer = m_vkDevice->createBufferUnique(bufferCInfo);

			auto bufMemReq = m_vkDevice->getBufferMemoryRequirements(m_vkConstBuffer.get());

			ATLASSERT(bufMemReq.alignment > 0);
			const auto alignedSizeInBytes = MyUtils::CalcAlignedSize(RequiredMemSizeInBytes, bufMemReq.alignment);
			ATLASSERT(bufMemReq.size == alignedSizeInBytes);

			vk::MemoryAllocateInfo memoryAInfo;
			memoryAInfo.allocationSize = bufMemReq.size;
			memoryAInfo.memoryTypeIndex = memoryTypeIndex;

			m_vkConstBufferMemory = m_vkDevice->allocateMemoryUnique(memoryAInfo);

			m_vkDevice->bindBufferMemory(m_vkConstBuffer.get(), m_vkConstBufferMemory.get(), 0);

			// 定数バッファはレンダリング ループにてフレームごとに書き換える。
		}

		// シェーダーの生成は Direct3D のようにサブスレッドでバックグラウンド実行したり、並列化したりできるはず。
		// C++11 であれば std::async で並列化するのがよさそう。
		{
			// HLSL ソースファイルの拡張子をいちいち GLSL 風に付け直すのは面倒なので、glslangValidator の "-S" コンパイル オプションを利用する。
			// シェーダーステージはファイル名のプレフィックスで区別している。

			m_vkPixelShader = CreateMyVkShaderModuleUniqueFromFile(m_vkDevice.get(), L"psSimple.spv");
			m_vkVertexShader1 = CreateMyVkShaderModuleUniqueFromFile(m_vkDevice.get(), L"vsSimple1.spv");
			m_vkVertexShader2 = CreateMyVkShaderModuleUniqueFromFile(m_vkDevice.get(), L"vsSimple2.spv");
		}

		// Direct3D 12 では Descriptor Heap が最難関だったが、Vulkan でも同様らしい。Descriptor Pool & Set まわりの理解が難しい。
		{
			vk::DescriptorPoolSize descPoolSize;
			descPoolSize.type = vk::DescriptorType::eUniformBuffer;
			descPoolSize.descriptorCount = 1;

			vk::DescriptorPoolCreateInfo descPoolCInfo;
			descPoolCInfo.poolSizeCount = 1;
			descPoolCInfo.pPoolSizes = &descPoolSize;
			descPoolCInfo.maxSets = 1;
			descPoolCInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

			m_vkDescPoolCB = m_vkDevice->createDescriptorPoolUnique(descPoolCInfo);
		}

		{
			vk::DescriptorSetLayoutBinding descSetLayoutBinding;
			descSetLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			descSetLayoutBinding.descriptorCount = 1;
			descSetLayoutBinding.binding = 0;
			descSetLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
			descSetLayoutBinding.pImmutableSamplers = nullptr;

			vk::DescriptorSetLayoutCreateInfo descSetLayoutCInfo;
			descSetLayoutCInfo.bindingCount = 1;
			descSetLayoutCInfo.pBindings = &descSetLayoutBinding;

			m_vkDescSetLayoutCB = m_vkDevice->createDescriptorSetLayoutUnique(descSetLayoutCInfo);
		}

		{
			vk::DescriptorSetAllocateInfo descSetAInfo;
			descSetAInfo.descriptorPool = m_vkDescPoolCB.get();
			descSetAInfo.descriptorSetCount = 1;
			descSetAInfo.pSetLayouts = &m_vkDescSetLayoutCB.get();

			m_vkDescSetsCB = m_vkDevice->allocateDescriptorSetsUnique(descSetAInfo);

			vk::DescriptorBufferInfo descBufferInfo;
			descBufferInfo.buffer = m_vkConstBuffer.get();
			descBufferInfo.range = VK_WHOLE_SIZE;

			vk::WriteDescriptorSet writeDescSet;
			writeDescSet.dstSet = m_vkDescSetsCB[0].get();
			writeDescSet.descriptorCount = 1;
			writeDescSet.descriptorType = vk::DescriptorType::eUniformBuffer;
			writeDescSet.pBufferInfo = &descBufferInfo;
			writeDescSet.dstBinding = 0;

			m_vkDevice->updateDescriptorSets(1, &writeDescSet, 0, nullptr);
		}

		{
			vk::PipelineLayoutCreateInfo pipelineLayoutCInfo;
			pipelineLayoutCInfo.setLayoutCount = 1;
			pipelineLayoutCInfo.pSetLayouts = &m_vkDescSetLayoutCB.get();

			m_vkPipelineLayoutCB = m_vkDevice->createPipelineLayoutUnique(pipelineLayoutCInfo);
		}

		{
			vk::VertexInputBindingDescription bindDesc;
			bindDesc.binding = 0;
			bindDesc.stride = sizeof(MyVertex);
			bindDesc.inputRate = vk::VertexInputRate::eVertex;

			vk::PipelineColorBlendAttachmentState blendState;
			blendState.colorWriteMask = vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eR;
			blendState.blendEnable = VK_FALSE;
			blendState.colorBlendOp = vk::BlendOp::eAdd;
			blendState.alphaBlendOp = vk::BlendOp::eAdd;
			blendState.srcColorBlendFactor = vk::BlendFactor::eOne;
			blendState.srcAlphaBlendFactor = vk::BlendFactor::eOne;
			blendState.dstColorBlendFactor = vk::BlendFactor::eZero;
			blendState.dstAlphaBlendFactor = vk::BlendFactor::eZero;

			m_vkSimpleGraphicsPipeline1 = CreateMyVkPipelineUnique(m_vkDevice.get(),
				m_vkPipelineLayoutCB.get(), m_vkRenderPass.get(),
				m_vkVertexShader1.get(), m_vkPixelShader.get(),
				m_viewport, m_rectScissor,
				blendState,
				bindDesc, InputAttributeDescMyVertex, std::size(InputAttributeDescMyVertex));

			blendState.blendEnable = VK_TRUE;
			blendState.dstColorBlendFactor = vk::BlendFactor::eOne;

			m_vkSimpleGraphicsPipeline2 = CreateMyVkPipelineUnique(m_vkDevice.get(),
				m_vkPipelineLayoutCB.get(), m_vkRenderPass.get(),
				m_vkVertexShader2.get(), m_vkPixelShader.get(),
				m_viewport, m_rectScissor,
				blendState,
				bindDesc, InputAttributeDescMyVertex, std::size(InputAttributeDescMyVertex));
		}

		{
			vk::CommandPoolCreateInfo commandPoolCInfo;
			commandPoolCInfo.queueFamilyIndex = queueFamilyIndex;
			commandPoolCInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

			m_vkCommandPool = m_vkDevice->createCommandPoolUnique(commandPoolCInfo);
		}

		{
			vk::CommandBufferAllocateInfo commandBufferAInfo;
			commandBufferAInfo.commandPool = m_vkCommandPool.get();
			commandBufferAInfo.level = vk::CommandBufferLevel::ePrimary;
			commandBufferAInfo.commandBufferCount = static_cast<uint32_t>(std::size(m_vkFrameBuffers));

			m_vkCommandBuffers = m_vkDevice->allocateCommandBuffersUnique(commandBufferAInfo);
		}

		{
			vk::FenceCreateInfo fenceCInfo;

			m_vkFence = m_vkDevice->createFenceUnique(fenceCInfo);
		}

		{
			m_vkImageBarriers.clear();
			for (const auto& image : m_vkSwapchainImages)
			{
				vk::ImageMemoryBarrier barrier;
				barrier.oldLayout = vk::ImageLayout::eUndefined;
				barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
				barrier.image = image;
				barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				barrier.subresourceRange.layerCount = 1;
				barrier.subresourceRange.levelCount = 1;
				m_vkImageBarriers.push_back(barrier);
			}

			auto& cmdBuffer = m_vkCommandBuffers[0];

			BeginMyVkCommandBuffer(cmdBuffer.get(), vk::Framebuffer());

			cmdBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(),
				0, nullptr, 0, nullptr, static_cast<uint32_t>(std::size(m_vkImageBarriers)), m_vkImageBarriers.data());

			cmdBuffer->end();

			SubmitMyVkCommandQueue(m_vkDeviceQueue, cmdBuffer.get(), vk::Fence());

			m_vkDeviceQueue.waitIdle();

			AcquireMyVkNextImageAndWait(m_vkDevice.get(), m_vkSwapchain.get(), m_vkFence.get(), UINT64_MAX, m_currentFrameIndex);
		}

		m_isInitialized = true;

		break;
	}
}

void MyVkManager::Render()
{
	if (!m_isInitialized)
	{
		return;
	}

	// 定数バッファの更新。
	{
		auto* pConstBuffer = static_cast<MyConstBuffer*>(m_vkDevice->mapMemory(m_vkConstBufferMemory.get(), 0, sizeof(MyConstBuffer)));
		ATLASSERT(pConstBuffer);

		DirectX::XMStoreFloat4x4(&pConstBuffer->UniWorld[0],
			DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationRollPitchYaw(0, 0, +m_vRotAngle.z), DirectX::XMMatrixTranslation(+0.3f, 0, 0))));
		DirectX::XMStoreFloat4x4(&pConstBuffer->UniWorld[1],
			DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(DirectX::XMMatrixRotationRollPitchYaw(0, 0, -m_vRotAngle.z), DirectX::XMMatrixTranslation(-0.3f, 0, 0))));

		// 縦横比の調整。
		// Vulkan の正規化デバイス座標系は、上方向が -Y、下方向が +Y となっているらしい。Direct3D/OpenGL とは異なるので注意。
		DirectX::XMStoreFloat4x4(&pConstBuffer->UniProjection,
			DirectX::XMMatrixTranspose(DirectX::XMMatrixScaling(m_viewport.height / m_viewport.width, -1, 1)));

		m_vkDevice->unmapMemory(m_vkConstBufferMemory.get());
		pConstBuffer = nullptr;
	}

	auto& cmdBuffer = m_vkCommandBuffers[0];

	ATLASSERT(m_currentFrameIndex < m_vkFrameBuffers.size());
	BeginMyVkCommandBuffer(cmdBuffer.get(), m_vkFrameBuffers[m_currentFrameIndex].get());
	BarrierMyVkResource(cmdBuffer.get(), m_vkSwapchainImages[m_currentFrameIndex],
		vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput,
		vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eColorAttachmentWrite,
		vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eColorAttachmentOptimal);
	const MyVector4F clearColor(DirectX::Colors::CornflowerBlue);
	const std::array<float, 4> clearColor4F{ clearColor.x, clearColor.y, clearColor.z, clearColor.w };
	// vk::ClearColorValue には initializer list を直接受け取るコンストラクタ オーバーロードが用意されていない。
	BeginMyVkRenderPass(cmdBuffer.get(), m_vkFrameBuffers[m_currentFrameIndex].get(), m_vkRenderPass.get(), m_renderArea, vk::ClearColorValue(clearColor4F));
	cmdBuffer->setViewport(0, 1, &m_viewport);
	cmdBuffer->setScissor(0, 1, &m_rectScissor);
	const vk::DeviceSize vbOffset = 0;
	cmdBuffer->bindVertexBuffers(0, 1, &m_vkVertexBuffer.get(), &vbOffset);
	cmdBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_vkPipelineLayoutCB.get(), 0, 1, &m_vkDescSetsCB[0].get(), 0, nullptr);
	cmdBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, m_vkSimpleGraphicsPipeline1.get());
	cmdBuffer->draw(3, 1, 0, 0);
	if (m_drawsBlendingObjects)
	{
		cmdBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, m_vkSimpleGraphicsPipeline2.get());
		cmdBuffer->draw(3, 2, 0, 0);
	}
	cmdBuffer->endRenderPass();
	cmdBuffer->end();

	SubmitMyVkCommandQueue(m_vkDeviceQueue, cmdBuffer.get(), m_vkFence.get());
	WaitForMyVkFence(m_vkDevice.get(), m_vkFence.get(), UINT64_MAX);
	PresentMyVkQueue(m_vkDeviceQueue, m_vkSwapchain.get(), m_currentFrameIndex);
	ResetMyVkFence(m_vkDevice.get(), m_vkFence.get());

	AcquireMyVkNextImageAndWait(m_vkDevice.get(), m_vkSwapchain.get(), m_vkFence.get(), UINT64_MAX, m_currentFrameIndex);
}

void MyVkManager::OnDestroy()
{
	if (m_vkDeviceQueue)
	{
		m_vkDeviceQueue.waitIdle();
	}
}
