#pragma once

#include "..\Common\DeviceResources.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MechaMan
{
	class SpriteTextureHeap;

	// A rectangle: where its top-left corner is, and how big it is.
	struct RectF
	{
		float x;
		float y;
		float width;
		float height;
	};

	extern const RectF c_view;

	// An image on the GPU that sprites are drawn from. Get one from SpriteRenderer::LoadTexture
	// or CreateTexture and keep it in a shared_ptr. It is a GPU object like any other: a state
	// creates its textures in CreateDeviceDependentResources and lets go of them in
	// ReleaseDeviceDependentResources. Letting go is safe at any time, even right after
	// drawing with the texture; the renderer keeps the image until the GPU has finished with it.
	class Texture
	{
	public:
		~Texture();

		UINT GetWidth() const { return m_width; }
		UINT GetHeight() const { return m_height; }

	private:
		friend class SpriteRenderer;

		Texture();
		Texture(const Texture&) = delete;
		Texture& operator=(const Texture&) = delete;

		std::shared_ptr<SpriteTextureHeap>		m_heap;
		Microsoft::WRL::ComPtr<ID3D12Resource>	m_resource;
		UINT									m_slot;
		UINT									m_width;
		UINT									m_height;
	};

	// One image to draw. Set the fields you need; left alone, the rest draw the whole
	// texture at its own size, unrotated and untinted, with its top-left corner at 'position'.
	struct Sprite
	{
		// The image. Leave it null to draw a plain rectangle in 'color'.
		const Texture*		texture = nullptr;

		// The part of the texture to draw, in pixels. All zeros means the whole texture.
		RectF				source = { 0.0f, 0.0f, 0.0f, 0.0f };

		// Where the sprite goes and how big it is drawn, in the units of the view passed to
		// Begin. A size of zero means the size of 'source'.
		DirectX::XMFLOAT2	position = { 0.0f, 0.0f };
		DirectX::XMFLOAT2	size = { 0.0f, 0.0f };

		// The point of the sprite that sits at 'position', and that it turns around:
		// (0, 0) is its top-left corner, (0.5, 0.5) its centre, (0.5, 1) the middle of its feet.
		DirectX::XMFLOAT2	origin = { 0.0f, 0.0f };

		// Clockwise, in radians.
		float				rotation = 0.0f;

		// Multiplies the image's colors: red, green, blue, opacity. White leaves it unchanged.
		DirectX::XMFLOAT4	color = { 1.0f, 1.0f, 1.0f, 1.0f };

		// Mirror the image.
		bool				flipX = false;
		bool				flipY = false;

		// What is drawn over what: a higher layer is drawn on top of a lower one. Within a
		// layer, sprites that share a texture are drawn in the order they were added. Sprites
		// in the same layer with different textures are drawn texture by texture, so if two
		// of them overlap and it matters which is on top, give them different layers.
		// From -32768 to 32767.
		int					layer = 0;
	};

	// Draws sprites in batches. Add them in any order between Begin and End; End sorts them by
	// layer and texture and draws each run of sprites that share a texture with one draw call.
	// One SpriteRenderer is shared by all the game states.
	class SpriteRenderer
	{
	public:
		SpriteRenderer();
		~SpriteRenderer();

		SpriteRenderer(const SpriteRenderer&) = delete;
		SpriteRenderer& operator=(const SpriteRenderer&) = delete;

		// GPU lifetime. MechaManMain calls these; states do not.
		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void ReleaseDeviceDependentResources();

		// MechaManMain calls this once at the start of every frame it renders, before any
		// state draws. It lets the renderer reuse the memory of frames the GPU has finished.
		void BeginFrame();

		// Textures. Both of these return once the image is on the GPU, which takes a moment:
		// make textures when a state is created, not every frame. They can also be called
		// from a loading task on another thread.
		//
		// A file name is relative to the app's package folder, for example
		// L"Assets\\Player.png". The file has to be in the project with Content set to Yes.
		// Any format Windows can decode works (PNG, BMP, JPEG, GIF).
		std::shared_ptr<Texture> LoadTexture(const std::wstring& fileName);

		// A texture from pixels in memory: 4 bytes per pixel (red, green, blue, opacity), top
		// row first, with the color bytes already multiplied by the opacity.
		std::shared_ptr<Texture> CreateTexture(UINT width, UINT height, const uint8_t* pixels);

		// Drawing. A state calls these from its Render:
		//
		//     m_sprites.Begin(view);
		//     for each thing on screen: m_sprites.Draw(sprite);
		//     m_sprites.End(commandList);
		//
		// 'view' is the rectangle of the game world that fills the screen; sprite positions
		// and sizes are in its units. Begin() without a view works in screen pixels.
		void Begin();
		void Begin(const RectF& view);
		void Draw(const Sprite& sprite);
		void End(ID3D12GraphicsCommandList* commandList);

		// What the last finished frame cost.
		UINT GetSpriteCount() const { return m_lastFrameSprites; }
		UINT GetDrawCallCount() const { return m_lastFrameDrawCalls; }

	private:
		// One corner of a sprite, as the vertex shader receives it.
		struct Vertex
		{
			DirectX::XMFLOAT2	position;
			DirectX::XMFLOAT2	textureCoordinate;
			uint32_t			color;
		};

		// The four corners of one sprite.
		struct Quad
		{
			Vertex corners[4];
		};

		// The vertices of one frame. There is one of these per back buffer, because the GPU
		// may still be drawing an earlier frame from its buffer while the next one is filled.
		struct FrameBuffer
		{
			Microsoft::WRL::ComPtr<ID3D12Resource>	resource;
			Quad*									mapped = nullptr;
			UINT									capacity = 0;	// in sprites
			UINT									used = 0;		// in sprites
		};

		void MakeRoomFor(UINT spriteCount);
		void CreatePipeline();
		void CreateIndexBuffer();

	private:
		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// Direct3D resources shared by every batch.
		Microsoft::WRL::ComPtr<ID3D12RootSignature>	m_rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>	m_pipelineState;
		Microsoft::WRL::ComPtr<ID3D12Resource>		m_indexBuffer;
		D3D12_INDEX_BUFFER_VIEW						m_indexBufferView;
		std::shared_ptr<SpriteTextureHeap>			m_textureHeap;
		std::shared_ptr<Texture>					m_whiteTexture;

		// This frame's vertices.
		FrameBuffer		m_frameBuffers[DX::c_frameCount];
		UINT			m_frameIndex;

		// The batch being built between Begin and End.
		bool					m_batchOpen;
		DirectX::XMFLOAT4X4		m_viewProjection;
		std::vector<Quad>		m_quads;	// in the order the sprites were added
		std::vector<uint64_t>	m_sortKeys;	// layer, texture, position in m_quads

		// Statistics.
		UINT	m_frameSprites;
		UINT	m_frameDrawCalls;
		UINT	m_lastFrameSprites;
		UINT	m_lastFrameDrawCalls;
	};
}
