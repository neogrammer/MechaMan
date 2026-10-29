#pragma once

#include "SpriteRenderer.h"

#include "DescriptorHeap.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"

#include <memory>
#include <string>

namespace MechaMan
{
	// Draws text with a DirectXTK sprite font. One is shared by all the states, like the
	// SpriteRenderer, and works the same way:
	//
	//     m_graphics.text.Begin(commandList);
	//     m_graphics.text.Draw(L"PRESS START", { 640.0f, 500.0f }, white, 1.0f, true);
	//     m_graphics.text.End();
	//
	// Positions are in the same view units as sprites (c_view unless you pass another view).
	// Text is drawn when End is called, so call it after the sprites' End to put text on top.
	// If the font file is missing, nothing is drawn and nothing fails.
	class TextRenderer
	{
	public:
		// GPU lifetime. MechaManMain calls these; states do not.
		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources, const std::wstring& fontFile);
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();

		bool HasFont() const { return m_font != nullptr; }

		void Begin(ID3D12GraphicsCommandList* commandList, const RectF& view = c_view);
		void Draw(const std::wstring& text, DirectX::XMFLOAT2 position,
			DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f },	// red, green, blue, opacity
			float scale = 1.0f,
			bool centred = false);										// true: 'position' is the middle of the text
		void End();

		// How big the text is, in view units.
		DirectX::XMFLOAT2 Measure(const std::wstring& text, float scale = 1.0f) const;

	private:
		std::shared_ptr<DX::DeviceResources>		m_deviceResources;
		std::unique_ptr<DirectX::DescriptorHeap>	m_descriptors;
		std::unique_ptr<DirectX::SpriteFont>		m_font;
		std::unique_ptr<DirectX::SpriteBatch>		m_batch;
		RectF										m_view = {};
		bool										m_batchOpen = false;
	};
}
