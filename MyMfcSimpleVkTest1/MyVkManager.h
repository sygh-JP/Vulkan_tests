#pragma once

// GLM ライブラリを使ってもよいが、Visual C++ (Windows SDK) に標準で用意されている DirectXMath を使うことにする。

using MyVector3F = DirectX::XMFLOAT3;
using MyVector4F = DirectX::XMFLOAT4;
using MyMatrix4x4F = DirectX::XMFLOAT4X4;

const MyMatrix4x4F IdentityMatrix4x4F(
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1);

class MyVertex final
{
public:
	MyVector3F Position;
	MyVector4F Color;
public:
	MyVertex()
		: Position(DirectX::g_XMZero)
		, Color(DirectX::g_XMZero)
	{}
	MyVertex(const MyVector3F& pos, const MyVector4F& color)
		: Position(pos)
		, Color(color)
	{}
};

// Direct3D の頂点レイアウトでは、オフセットサイズを API に自動計算させることができたが、Vulkan では明示的に指定するしかないらしい？
// Direct3D では各属性に HLSL セマンティクスを文字列として指定するが、Vulkan ではセマンティクスを指定することができない。
// GLSL を使う場合は location をもとに関連付けているはずだが、
// HLSL 側の SV_Position とはどうやって関連付けているのか（どうやってワールド座標の float3 から rhw の float4 にマッピングしているのか）詳細不明。
const vk::VertexInputAttributeDescription InputAttributeDescMyVertex[] =
{
	{ 0, 0, vk::Format::eR32G32B32Sfloat, 0 }, // Position
	{ 1, 0, vk::Format::eR32G32B32A32Sfloat, sizeof(float) * 3 }, // Color
};

struct MyConstBuffer
{
	MyMatrix4x4F UniWorld[2];
	MyMatrix4x4F UniProjection;
};

// あえて Direct3D 風に定数バッファおよびピクセルシェーダーという用語を使用している。ご了承を。

class MyVkManager
{
private:
	vk::UniqueInstance m_vkInstance;
	vk::UniqueDebugReportCallbackEXT m_vkDebugReportCallback;
	vk::UniqueDevice m_vkDevice;
	vk::Queue m_vkDeviceQueue;
	vk::UniqueSurfaceKHR m_vkSurface;
	vk::UniqueSwapchainKHR m_vkSwapchain;
	std::vector<vk::Image> m_vkSwapchainImages;
	std::vector<vk::UniqueImageView> m_vkImageViews;
	std::vector<vk::ImageMemoryBarrier> m_vkImageBarriers;
	vk::UniqueRenderPass m_vkRenderPass;
	std::vector<vk::UniqueFramebuffer> m_vkFrameBuffers;
	vk::UniqueDeviceMemory m_vkVertexBufferMemory;
	vk::UniqueBuffer m_vkVertexBuffer;
	vk::UniqueDeviceMemory m_vkConstBufferMemory;
	vk::UniqueBuffer m_vkConstBuffer;
	vk::UniqueShaderModule m_vkPixelShader;
	vk::UniqueShaderModule m_vkVertexShader1;
	vk::UniqueShaderModule m_vkVertexShader2;
	vk::UniqueDescriptorPool m_vkDescPoolCB;
	vk::UniqueDescriptorSetLayout m_vkDescSetLayoutCB;
	std::vector<vk::UniqueDescriptorSet> m_vkDescSetsCB;
	vk::UniquePipelineLayout m_vkPipelineLayoutCB;
	vk::UniquePipeline m_vkSimpleGraphicsPipeline1;
	vk::UniquePipeline m_vkSimpleGraphicsPipeline2;
	vk::UniqueCommandPool m_vkCommandPool;
	std::vector<vk::UniqueCommandBuffer> m_vkCommandBuffers;
	vk::UniqueFence m_vkFence;
	vk::Viewport m_viewport;
	vk::Rect2D m_rectScissor;
	vk::Rect2D m_renderArea;

	uint32_t m_currentFrameIndex = 0;
private:
	bool m_isInitialized = false;
	MyVector3F m_vRotAngle = MyVector3F(DirectX::g_XMZero);
	bool m_drawsBlendingObjects = true;
public:
	MyVkManager()
	{}
	virtual ~MyVkManager()
	{
		try
		{
			this->OnDestroy();
		}
		catch (const std::exception& ex)
		{
			const CString strTypeName(typeid(ex).name());
			const CString strErr(ex.what());
			ATLTRACE(_T("Exception=%s, Message=\"%s\"\n"), strTypeName.GetString(), strErr.GetString());
		}
	}
public:
	void Init(HWND hWnd, UINT width, UINT height);
	void Render();
public:
	MyVector3F GetRotAngle() const { return m_vRotAngle; }
	void SetRotAngle(const MyVector3F& inVal) { m_vRotAngle = inVal; }
	bool GetDrawsBlendingObjects() const { return m_drawsBlendingObjects; }
	void SetDrawsBlendingObjects(bool inVal) { m_drawsBlendingObjects = inVal; }
private:
	//void LoadPipeline(HWND hWnd, UINT width, UINT height);
	//void LoadAssets(UINT width, UINT height);
	//void WaitForGPU();
	//void PopulateCommandLists();
	void OnDestroy();
};
