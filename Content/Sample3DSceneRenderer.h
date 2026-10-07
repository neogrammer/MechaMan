#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"

#include <atomic>
#include <memory>
#include <vector>

namespace MechaMan
{
	// Owns the GPU objects for the sample cube and draws it into the frame's command list.
	// It holds no game data: the state that owns it says how far the cube has turned.
	class Sample3DSceneRenderer : public std::enable_shared_from_this<Sample3DSceneRenderer>
	{
	public:
		// Creates the renderer and starts loading its resources in the background.
		static std::shared_ptr<Sample3DSceneRenderer> Create(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~Sample3DSceneRenderer();

		void CreateWindowSizeDependentResources();

		// Loading is asynchronous. Render draws nothing until this returns true.
		bool IsLoaded() const { return m_loadingComplete.load(); }

		// Rotate the 3D cube model a set amount of radians.
		void Rotate(float radians);

		// Records the cube's draw calls. The caller owns the frame: it has already set the
		// viewport, scissor rectangle and render targets, and it executes the command list.
		void Render(ID3D12GraphicsCommandList* commandList);

	private:
		Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateDeviceDependentResources();

	private:
		// Constant buffers must be 256-byte aligned.
		static const UINT c_alignedConstantBufferSize = (sizeof(ModelViewProjectionConstantBuffer) + 255) & ~255;

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// Direct3D resources for cube geometry.
		Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_cbvHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_constantBuffer;
		ModelViewProjectionConstantBuffer					m_constantBufferData;
		UINT8*												m_mappedConstantBuffer;
		UINT												m_cbvDescriptorSize;
		std::vector<byte>									m_vertexShader;
		std::vector<byte>									m_pixelShader;
		D3D12_VERTEX_BUFFER_VIEW							m_vertexBufferView;
		D3D12_INDEX_BUFFER_VIEW								m_indexBufferView;

		// Set by the loading tasks, which may run on another thread, and read by the game loop.
		std::atomic<bool>	m_loadingComplete;
	};
}
