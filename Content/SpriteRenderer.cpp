#include "pch.h"
#include "SpriteRenderer.h"

#include "..\Common\DirectXHelper.h"
#include "PackageFiles.h"

#include <algorithm>
#include <cassert>
#include <mutex>

using namespace MechaMan;

using namespace DirectX;
using namespace Microsoft::WRL;


const MechaMan::RectF MechaMan::c_view = { 0.0f, 0.0f, 1280.0f, 720.0f };

namespace
{
	

	// One draw call's indices are 16 bits wide, which is enough for this many sprites.
	const UINT c_maxSpritesPerDraw = 16384;

	// A frame's vertex buffer starts with room for this many sprites and grows when needed.
	const UINT c_initialFrameCapacity = 2048;

	// How many textures can exist at once.
	const UINT c_maxTextures = 1024;

	// Root signature slots.
	const UINT c_rootViewProjection = 0;
	const UINT c_rootTexture = 1;

	// What a sort key is made of, most significant first: layer, texture, order added.
	const int c_minLayer = -32768;
	const int c_maxLayer = 32767;

	uint64_t MakeSortKey(int layer, UINT textureSlot, size_t order)
	{
		return (static_cast<uint64_t>(layer - c_minLayer) << 48) | (static_cast<uint64_t>(textureSlot) << 32) | static_cast<uint64_t>(order);
	}

	UINT TextureSlotOf(uint64_t sortKey)
	{
		return static_cast<UINT>((sortKey >> 32) & 0xFFFF);
	}

	size_t OrderOf(uint64_t sortKey)
	{
		return static_cast<size_t>(sortKey & 0xFFFFFFFF);
	}

	float Saturate(float value)
	{
		return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
	}

	// A tint as the vertex buffer stores it: multiplied by its opacity, 8 bits per channel,
	// in the byte order of DXGI_FORMAT_R8G8B8A8_UNORM.
	uint32_t PackColor(const XMFLOAT4& color)
	{
		const float opacity = Saturate(color.w);
		const uint32_t red = static_cast<uint32_t>(Saturate(color.x) * opacity * 255.0f + 0.5f);
		const uint32_t green = static_cast<uint32_t>(Saturate(color.y) * opacity * 255.0f + 0.5f);
		const uint32_t blue = static_cast<uint32_t>(Saturate(color.z) * opacity * 255.0f + 0.5f);
		const uint32_t alpha = static_cast<uint32_t>(opacity * 255.0f + 0.5f);
		return red | (green << 8) | (blue << 16) | (alpha << 24);
	}

	// Copies pixels into a texture and waits until the GPU has done it. This uses a command
	// allocator, command list and fence of its own, so it never touches the ones the frame is
	// being recorded with.
	void UploadPixels(DX::DeviceResources& deviceResources, ID3D12Resource* texture, UINT width, UINT height, const uint8_t* pixels)
	{
		auto d3dDevice = deviceResources.GetD3DDevice();

		// The pixels go to the GPU by way of a buffer in the upload heap.
		ComPtr<ID3D12Resource> upload;
		CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(GetRequiredIntermediateSize(texture, 0, 1));
		DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&upload)));

		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList> commandList;
		DX::ThrowIfFailed(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
		DX::ThrowIfFailed(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = pixels;
		data.RowPitch = static_cast<LONG_PTR>(width) * 4;
		data.SlicePitch = data.RowPitch * height;
		if (UpdateSubresources(commandList.Get(), texture, upload.Get(), 0, 0, 1, &data) == 0)
		{
			// The copy could not be set up, and the texture would be left blank.
			DX::ThrowIfFailed(E_FAIL);
		}

		CD3DX12_RESOURCE_BARRIER barrier =
			CD3DX12_RESOURCE_BARRIER::Transition(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);

		DX::ThrowIfFailed(commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
		deviceResources.GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// Wait for the copy; the upload buffer has to stay alive until it is done.
		ComPtr<ID3D12Fence> fence;
		DX::ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
		DX::ThrowIfFailed(deviceResources.GetCommandQueue()->Signal(fence.Get(), 1));

		HANDLE finished = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (finished == nullptr)
		{
			DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		HRESULT hr = fence->SetEventOnCompletion(1, finished);
		if (SUCCEEDED(hr))
		{
			WaitForSingleObjectEx(finished, INFINITE, FALSE);
		}
		CloseHandle(finished);
		DX::ThrowIfFailed(hr);
	}
}

namespace MechaMan
{
	// The descriptor heap that holds every sprite texture's view, and the place GPU objects
	// wait, after the game has let go of them, until the GPU has finished drawing with them.
	// The renderer and every texture hold a reference to it, so it outlives whichever goes first.
	class SpriteTextureHeap
	{
	public:
		static const UINT c_noSlot = 0xFFFFFFFF;

		SpriteTextureHeap(ID3D12Device* d3dDevice, UINT capacity)
		{
			// This flag indicates that this descriptor heap can be bound to the pipeline and that descriptors contained in it can be referenced by a root table.
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.NumDescriptors = capacity;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			DX::ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap)));
			NAME_D3D12_OBJECT(m_heap);

			m_descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// Hand out the lowest slots first.
			for (UINT slot = capacity; slot > 0; slot--)
			{
				m_freeSlots.push_back(slot - 1);
			}
		}

		ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(UINT slot) const
		{
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(slot), m_descriptorSize);
		}

		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(UINT slot) const
		{
			return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(slot), m_descriptorSize);
		}

		// A place in the heap for one more texture's view.
		UINT Allocate()
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			if (m_freeSlots.empty())
			{
				OutputDebugStringW(L"SpriteRenderer: too many textures are alive at once. The limit is c_maxTextures in SpriteRenderer.cpp.\n");
				DX::ThrowIfFailed(E_OUTOFMEMORY);
			}

			UINT slot = m_freeSlots.back();
			m_freeSlots.pop_back();
			return slot;
		}

		// Takes over a GPU object the game is done with. Frames that are already on their way
		// to the screen may still use it, so it is kept until they have been drawn.
		void Retire(ComPtr<ID3D12Resource> resource, UINT slot)
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			if (m_heap == nullptr)
			{
				// The device is gone and nothing is being drawn. Let go straight away.
				return;
			}

			Retired retired;
			retired.resource = resource;
			retired.slot = slot;
			retired.framesLeft = DX::c_frameCount;
			m_retired.push_back(retired);
		}

		// Called at the start of each frame. By the time DX::c_frameCount frames have started
		// since an object was retired, DeviceResources has waited for every frame that could
		// have used it.
		void FrameStarted()
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			size_t kept = 0;
			for (size_t i = 0; i < m_retired.size(); i++)
			{
				m_retired[i].framesLeft--;
				if (m_retired[i].framesLeft > 0)
				{
					if (kept != i)
					{
						m_retired[kept] = m_retired[i];
					}
					kept++;
				}
				else if (m_retired[i].slot != c_noSlot)
				{
					m_freeSlots.push_back(m_retired[i].slot);
				}
			}
			m_retired.resize(kept);
		}

		// The device is going away. Let go of everything now.
		void Shutdown()
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			m_retired.clear();
			m_freeSlots.clear();
			m_heap = nullptr;
		}

	private:
		struct Retired
		{
			ComPtr<ID3D12Resource>	resource;
			UINT					slot;
			UINT					framesLeft;
		};

		// Textures can be let go of from any thread.
		std::mutex						m_mutex;
		ComPtr<ID3D12DescriptorHeap>	m_heap;
		UINT							m_descriptorSize;
		std::vector<UINT>				m_freeSlots;
		std::vector<Retired>			m_retired;
	};
}

Texture::Texture() :
	m_slot(SpriteTextureHeap::c_noSlot),
	m_width(0),
	m_height(0)
{
}

Texture::~Texture()
{
	// The GPU may still be drawing with this image. The heap keeps it until it has finished.
	if (m_heap != nullptr)
	{
		m_heap->Retire(m_resource, m_slot);
	}
}

SpriteRenderer::SpriteRenderer() :
	m_indexBufferView(),
	m_frameIndex(0),
	m_batchOpen(false),
	m_frameSprites(0),
	m_frameDrawCalls(0),
	m_lastFrameSprites(0),
	m_lastFrameDrawCalls(0)
{
	XMStoreFloat4x4(&m_viewProjection, XMMatrixIdentity());
}

SpriteRenderer::~SpriteRenderer()
{
	ReleaseDeviceDependentResources();
}

// Called at startup, and again after the device has been lost and re-created.
void SpriteRenderer::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	m_deviceResources = deviceResources;
	m_textureHeap = std::make_shared<SpriteTextureHeap>(deviceResources->GetD3DDevice(), c_maxTextures);

	CreatePipeline();
	CreateIndexBuffer();

	// One white pixel: what a sprite without a texture is drawn from.
	const uint8_t white[] = { 255, 255, 255, 255 };
	m_whiteTexture = CreateTexture(1, 1, white);

	

}

// The device is gone. Every state has to let go of its textures as well.
void SpriteRenderer::ReleaseDeviceDependentResources()
{
	m_whiteTexture = nullptr;
	m_textureCache.clear();

	for (FrameBuffer& frameBuffer : m_frameBuffers)
	{
		frameBuffer = FrameBuffer();
	}

	m_indexBuffer = nullptr;
	m_pipelineState = nullptr;
	m_rootSignature = nullptr;

	if (m_textureHeap != nullptr)
	{
		m_textureHeap->Shutdown();
		m_textureHeap = nullptr;
	}

	m_deviceResources = nullptr;

	m_frameIndex = 0;
	m_batchOpen = false;
	m_quads.clear();
	m_sortKeys.clear();
}

void SpriteRenderer::CreatePipeline()
{
	auto d3dDevice = m_deviceResources->GetD3DDevice();

	// Create a root signature with the view matrix for the vertex shader, and one texture
	// and a sampler for the pixel shader.
	{
		CD3DX12_DESCRIPTOR_RANGE textureRange;
		textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER parameters[2];
		parameters[c_rootViewProjection].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		parameters[c_rootTexture].InitAsDescriptorTable(1, &textureRange, D3D12_SHADER_VISIBILITY_PIXEL);

		// Point sampling keeps pixel art sharp when it is scaled up.
		// For smooth scaling instead, use D3D12_FILTER_MIN_MAG_MIP_LINEAR.
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.MipLODBias = 0.0f;
		sampler.MaxAnisotropy = 1;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

		CD3DX12_ROOT_SIGNATURE_DESC descRootSignature;
		descRootSignature.Init(_countof(parameters), parameters, 1, &sampler, rootSignatureFlags);

		ComPtr<ID3DBlob> pSignature;
		ComPtr<ID3DBlob> pError;
		DX::ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, pSignature.GetAddressOf(), pError.GetAddressOf()));
		DX::ThrowIfFailed(d3dDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
		NAME_D3D12_OBJECT(m_rootSignature);
	}

	// Create the pipeline state from the two compiled shaders.
	{
		std::vector<uint8_t> vertexShader = PackageFiles::ReadBytes(L"SpriteVertexShader.cso");
		std::vector<uint8_t> pixelShader = PackageFiles::ReadBytes(L"SpritePixelShader.cso");

		static const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC state = {};
		state.InputLayout = { inputLayout, _countof(inputLayout) };
		state.pRootSignature = m_rootSignature.Get();
		state.VS = CD3DX12_SHADER_BYTECODE(vertexShader.data(), vertexShader.size());
		state.PS = CD3DX12_SHADER_BYTECODE(pixelShader.data(), pixelShader.size());

		// A mirrored sprite has its corners in the opposite order, so draw both sides.
		state.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		// Blend each sprite over what is already there. The colors arrive already multiplied
		// by their opacity, so the source is added as it is.
		state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		state.BlendState.RenderTarget[0].BlendEnable = TRUE;
		state.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		state.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		state.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		state.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		state.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		state.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

		// Sprites ignore the depth buffer: whatever is drawn later is on top.
		state.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		state.DepthStencilState.DepthEnable = FALSE;
		state.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		state.SampleMask = UINT_MAX;
		state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		state.NumRenderTargets = 1;
		state.RTVFormats[0] = m_deviceResources->GetBackBufferFormat();
		state.DSVFormat = m_deviceResources->GetDepthBufferFormat();
		state.SampleDesc.Count = 1;

		HRESULT hr = d3dDevice->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&m_pipelineState));
		if (FAILED(hr))
		{
			OutputDebugStringW(L"SpriteRenderer: the sprite shaders were rejected. In the Properties of SpriteVertexShader.hlsl and SpritePixelShader.hlsl, check that Shader Type is Vertex Shader and Pixel Shader.\n");
		}
		DX::ThrowIfFailed(hr);
		NAME_D3D12_OBJECT(m_pipelineState);
	}
}

void SpriteRenderer::CreateIndexBuffer()
{
	auto d3dDevice = m_deviceResources->GetD3DDevice();

	// Two triangles per sprite, for as many sprites as one draw call can hold. The buffer is
	// written once and never changes, so it can simply stay in the upload heap.
	const UINT indexBufferSize = c_maxSpritesPerDraw * 6 * sizeof(uint16_t);

	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
	DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&indexBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_indexBuffer)));

	NAME_D3D12_OBJECT(m_indexBuffer);

	uint16_t* indices = nullptr;
	CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
	DX::ThrowIfFailed(m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&indices)));

	for (UINT sprite = 0; sprite < c_maxSpritesPerDraw; sprite++)
	{
		// A sprite's corners: 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right.
		const uint16_t first = static_cast<uint16_t>(sprite * 4);
		indices[0] = first;
		indices[1] = first + 1;
		indices[2] = first + 2;
		indices[3] = first + 2;
		indices[4] = first + 1;
		indices[5] = first + 3;
		indices += 6;
	}

	m_indexBuffer->Unmap(0, nullptr);

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.SizeInBytes = indexBufferSize;
	m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
}

std::shared_ptr<Texture> SpriteRenderer::LoadTexture(const std::wstring& fileName)
{
	PackageFiles::Image image = PackageFiles::ReadImage(fileName);
	return CreateTexture(image.width, image.height, image.pixels.data());
}

std::shared_ptr<Texture> SpriteRenderer::CreateTexture(UINT width, UINT height, const uint8_t* pixels)
{
	if (m_deviceResources == nullptr || width == 0 || height == 0 || pixels == nullptr)
	{
		DX::ThrowIfFailed(E_INVALIDARG);
	}

	auto d3dDevice = m_deviceResources->GetD3DDevice();

	std::shared_ptr<Texture> texture(new Texture());
	texture->m_width = width;
	texture->m_height = height;

	// The texture lives in the GPU's default heap, and starts out ready to be copied into.
	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
	DX::ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texture->m_resource)));

	UploadPixels(*m_deviceResources, texture->m_resource.Get(), width, height, pixels);

	// Give it a view in the heap the pixel shader reads textures from.
	texture->m_slot = m_textureHeap->Allocate();
	texture->m_heap = m_textureHeap;
	d3dDevice->CreateShaderResourceView(texture->m_resource.Get(), nullptr, m_textureHeap->GetCpuHandle(texture->m_slot));

	return texture;
}

// Gets ready for a new frame. MechaManMain calls this once at the start of every frame it renders.
void SpriteRenderer::BeginFrame()
{
	assert(!m_batchOpen);	// a Begin in the last frame has no End

	// DeviceResources has waited for the frame that last used this back buffer, so the GPU has
	// finished with the sprites this buffer held and it can be filled from the start again.
	m_frameIndex = m_deviceResources->GetCurrentFrameIndex();
	m_frameBuffers[m_frameIndex].used = 0;

	// One frame closer to letting go of the textures the game has dropped.
	m_textureHeap->FrameStarted();

	m_lastFrameSprites = m_frameSprites;
	m_lastFrameDrawCalls = m_frameDrawCalls;
	m_frameSprites = 0;
	m_frameDrawCalls = 0;
}

// Makes sure this frame's vertex buffer can take that many more sprites.
void SpriteRenderer::MakeRoomFor(UINT spriteCount)
{
	FrameBuffer& frameBuffer = m_frameBuffers[m_frameIndex];
	if (frameBuffer.used + spriteCount <= frameBuffer.capacity)
	{
		return;
	}

	UINT capacity = frameBuffer.capacity * 2;
	if (capacity < c_initialFrameCapacity)
	{
		capacity = c_initialFrameCapacity;
	}
	if (capacity < spriteCount)
	{
		capacity = spriteCount;
	}

	// Batches drawn earlier in this frame still use the old buffer, so it has to last until
	// the GPU has drawn them. This batch starts a new, bigger one.
	if (frameBuffer.resource != nullptr)
	{
		m_textureHeap->Retire(frameBuffer.resource, SpriteTextureHeap::c_noSlot);
		frameBuffer = FrameBuffer();
	}

	CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(capacity) * sizeof(Quad));
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&frameBuffer.resource)));

	// Keeping things mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
	DX::ThrowIfFailed(frameBuffer.resource->Map(0, &readRange, reinterpret_cast<void**>(&frameBuffer.mapped)));

	frameBuffer.capacity = capacity;
	frameBuffer.used = 0;
}

// Starts a batch in screen pixels: (0, 0) is the top-left corner of the game's part of the
// screen, inside any black bars.
void SpriteRenderer::Begin()
{
	const D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	Begin(RectF{ 0.0f, 0.0f, viewport.Width, viewport.Height });
}

// Starts a batch. 'view' is the rectangle of the game world that fills the screen.
void SpriteRenderer::Begin(const RectF& view)
{
	assert(!m_batchOpen);	// the previous Begin has no End
	assert(m_frameIndex == m_deviceResources->GetCurrentFrameIndex());	// BeginFrame was not called this frame

	// The view's top-left corner goes to the top-left corner of the screen and its
	// bottom-right corner to the bottom-right, with y pointing down.
	XMMATRIX projection = XMMatrixOrthographicOffCenterLH(view.x, view.x + view.width, view.y + view.height, view.y, 0.0f, 1.0f);

	// As for any draw to the swap chain, post-multiply by the display orientation.
	XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();
	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

	XMStoreFloat4x4(&m_viewProjection, XMMatrixTranspose(projection * orientationMatrix));

	m_quads.clear();
	m_sortKeys.clear();
	m_batchOpen = true;
}

// Adds one sprite to the batch. Nothing is drawn until End.
void SpriteRenderer::Draw(const Sprite& sprite)
{
	assert(m_batchOpen);	// Draw only works between Begin and End
	if (!m_batchOpen)
	{
		return;
	}

	// No texture means a plain rectangle, which is drawn from a single white pixel.
	// (So is a texture from before the device was lost, which cannot be drawn from any more.)
	const Texture* texture = sprite.texture;
	if (texture == nullptr || texture->m_heap != m_textureHeap)
	{
		assert(texture == nullptr);
		texture = m_whiteTexture.get();
	}
	if (texture == nullptr)
	{
		return;
	}

	// The part of the texture to draw, as fractions of its size.
	const float textureWidth = static_cast<float>(texture->m_width);
	const float textureHeight = static_cast<float>(texture->m_height);

	RectF source = {
		static_cast<float>(sprite.texRect.x), static_cast<float>(sprite.texRect.y),
		static_cast<float>(sprite.texRect.width), static_cast<float>(sprite.texRect.height) };
	if (sprite.texRect.width == 0 && sprite.texRect.height == 0)
	{
		source = RectF{ 0.0f, 0.0f, textureWidth, textureHeight };
	}

	float left = source.x / textureWidth;
	float right = (source.x + source.width) / textureWidth;
	float top = source.y / textureHeight;
	float bottom = (source.y + source.height) / textureHeight;

	if (sprite.flipX)
	{
		std::swap(left, right);
	}
	if (sprite.flipY)
	{
		std::swap(top, bottom);
	}

	// The sprite's corners relative to its origin.
	float width = sprite.size.x;
	float height = sprite.size.y;
	if (width == 0.0f && height == 0.0f)
	{
		width = source.width;
		height = source.height;
	}

	const float x0 = -sprite.origin.x * width;
	const float y0 = -sprite.origin.y * height;
	const float x1 = x0 + width;
	const float y1 = y0 + height;

	// Turned around the origin (clockwise, as y points down), then moved into place.
	float cosine = 1.0f;
	float sine = 0.0f;
	if (sprite.rotation != 0.0f)
	{
		XMScalarSinCos(&sine, &cosine, sprite.rotation);
	}

	const uint32_t color = PackColor(sprite.color);

	Quad quad;
	const float cornerX[4] = { x0, x1, x0, x1 };
	const float cornerY[4] = { y0, y0, y1, y1 };
	const float cornerU[4] = { left, right, left, right };
	const float cornerV[4] = { top, top, bottom, bottom };
	for (int corner = 0; corner < 4; corner++)
	{
		quad.corners[corner].position.x = sprite.position.x + cornerX[corner] * cosine - cornerY[corner] * sine;
		quad.corners[corner].position.y = sprite.position.y + cornerX[corner] * sine + cornerY[corner] * cosine;
		quad.corners[corner].textureCoordinate.x = cornerU[corner];
		quad.corners[corner].textureCoordinate.y = cornerV[corner];
		quad.corners[corner].color = color;
	}

	int layer = sprite.layer;
	if (layer < c_minLayer)
	{
		layer = c_minLayer;
	}
	if (layer > c_maxLayer)
	{
		layer = c_maxLayer;
	}

	m_sortKeys.push_back(MakeSortKey(layer, texture->m_slot, m_quads.size()));
	m_quads.push_back(quad);
}

// Sorts the batch and records its draw calls: one for each run of sprites that share a texture.
void SpriteRenderer::End(ID3D12GraphicsCommandList* commandList)
{
	assert(m_batchOpen);	// End without Begin
	m_batchOpen = false;

	const UINT spriteCount = static_cast<UINT>(m_quads.size());
	if (spriteCount == 0)
	{
		return;
	}

	// By layer, then by texture, then in the order the sprites were added.
	std::sort(m_sortKeys.begin(), m_sortKeys.end());

	// Copy the corners into this frame's vertex buffer in the order they will be drawn.
	MakeRoomFor(spriteCount);
	FrameBuffer& frameBuffer = m_frameBuffers[m_frameIndex];
	const UINT firstSprite = frameBuffer.used;
	for (UINT i = 0; i < spriteCount; i++)
	{
		frameBuffer.mapped[firstSprite + i] = m_quads[OrderOf(m_sortKeys[i])];
	}
	frameBuffer.used += spriteCount;

	PIXBeginEvent(commandList, 0, L"Draw sprites");
	{
		// Set the pipeline state, graphics root signature and descriptor heaps sprites are drawn with.
		commandList->SetPipelineState(m_pipelineState.Get());
		commandList->SetGraphicsRootSignature(m_rootSignature.Get());
		ID3D12DescriptorHeap* ppHeaps[] = { m_textureHeap->GetHeap() };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		// The view goes to the vertex shader as 16 constants in the root signature.
		commandList->SetGraphicsRoot32BitConstants(c_rootViewProjection, 16, &m_viewProjection, 0);

		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		vertexBufferView.BufferLocation = frameBuffer.resource->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes = sizeof(Vertex);
		vertexBufferView.SizeInBytes = static_cast<UINT>(frameBuffer.capacity * sizeof(Quad));

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&m_indexBufferView);

		// One draw call for each run of sprites that share a texture.
		UINT runStart = 0;
		while (runStart < spriteCount)
		{
			const UINT textureSlot = TextureSlotOf(m_sortKeys[runStart]);

			UINT runEnd = runStart + 1;
			while (runEnd < spriteCount && TextureSlotOf(m_sortKeys[runEnd]) == textureSlot)
			{
				runEnd++;
			}

			commandList->SetGraphicsRootDescriptorTable(c_rootTexture, m_textureHeap->GetGpuHandle(textureSlot));

			// A very long run is split, because the index buffer only reaches so far.
			for (UINT start = runStart; start < runEnd; start += c_maxSpritesPerDraw)
			{
				UINT count = runEnd - start;
				if (count > c_maxSpritesPerDraw)
				{
					count = c_maxSpritesPerDraw;
				}

				commandList->DrawIndexedInstanced(count * 6, 1, 0, static_cast<INT>((firstSprite + start) * 4), 0);
				m_frameDrawCalls++;
			}

			runStart = runEnd;
		}
	}
	PIXEndEvent(commandList);

	m_frameSprites += spriteCount;

	m_quads.clear();
	m_sortKeys.clear();
}

// ---- The simple way ---------------------------------------------------------------------

RectF MechaMan::FitInside(UINT width, UINT height, const RectF& area, float fill)
{
	if (width == 0 || height == 0)
	{
		return RectF{ area.x + area.width * 0.5f, area.y + area.height * 0.5f, 0.0f, 0.0f };
	}

	// Scale by whichever side runs out of room first.
	const float scaleX = area.width / static_cast<float>(width);
	const float scaleY = area.height / static_cast<float>(height);
	const float scale = (scaleX < scaleY ? scaleX : scaleY) * fill;

	const float w = static_cast<float>(width) * scale;
	const float h = static_cast<float>(height) * scale;
	return RectF{ area.x + (area.width - w) * 0.5f, area.y + (area.height - h) * 0.5f, w, h };
}

void SpriteRenderer::DrawTexture(const Texture* texture, DirectX::XMFLOAT2 position, float opacity, int layer)
{
	if (texture == nullptr)
	{
		return;
	}
	DrawTexture(texture, RectF{ position.x, position.y, static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight()) }, opacity, layer);
}

void SpriteRenderer::DrawTexture(const Texture* texture, const RectF& destination, float opacity, int layer)
{
	// A source of all zeros means the whole texture.
	DrawTexture(texture, Rect{ 0, 0, 0, 0 }, destination, opacity, layer);
}

void SpriteRenderer::DrawTexture(Sprite& sprite, const RectF& destination, float opacity, int layer, bool resetTexRect)
{
	if (sprite.texture == nullptr)
	{
		return;
	}
	if (resetTexRect)
	{
		sprite.ResetTexRect();
	}
	if (opacity <= 0.0f)
	{
		return;
	}

	// The sprite's own texture, texRect and mirroring, put where the arguments say.
	Sprite placed;
	placed.texture = sprite.texture;
	placed.texRect = sprite.texRect;
	placed.flipX = sprite.flipX;
	placed.flipY = sprite.flipY;
	placed.position = { destination.x, destination.y };
	placed.size = { destination.width, destination.height };
	placed.color = { 1.0f, 1.0f, 1.0f, opacity };
	placed.layer = layer;
	Draw(placed);
}

void SpriteRenderer::DrawTexture(const Texture* texture, const Rect& source, const RectF& destination, float opacity, int layer)
{
	if (texture == nullptr || opacity <= 0.0f)
	{
		return;
	}

	Sprite sprite;
	sprite.texture = texture;
	sprite.texRect = source;
	sprite.position = { destination.x, destination.y };
	sprite.size = { destination.width, destination.height };
	sprite.color = { 1.0f, 1.0f, 1.0f, opacity };
	sprite.layer = layer;
	Draw(sprite);
}

void SpriteRenderer::DrawRectangle(const RectF& rectangle, DirectX::XMFLOAT4 color, int layer)
{
	Sprite sprite;	// no texture: a plain rectangle
	sprite.position = { rectangle.x, rectangle.y };
	sprite.size = { rectangle.width, rectangle.height };
	sprite.color = color;
	sprite.layer = layer;
	Draw(sprite);
}

std::shared_ptr<Texture> SpriteRenderer::GetTexture(const std::wstring& fileName)
{
	auto found = m_textureCache.find(fileName);
	if (found != m_textureCache.end())
	{
		return found->second;
	}

	std::shared_ptr<Texture> texture = LoadTexture(fileName);
	m_textureCache[fileName] = texture;
	return texture;
}
