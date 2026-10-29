#include "pch.h"
#include "TextRenderer.h"

#include "..\Common\DirectXHelper.h"
#include "PackageFiles.h"

#include "ResourceUploadBatch.h"

#include <cstring>

using namespace MechaMan;
using namespace DirectX;

void TextRenderer::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources, const std::wstring& fontFile)
{
	m_deviceResources = deviceResources;

	// The font is optional: without it the game still runs, it just shows no text.
	if (!PackageFiles::Exists(fontFile))
	{
		OutputDebugStringW((L"TextRenderer: no font file " + fontFile + L" in the package; text will not be drawn.\n").c_str());
		return;
	}

	std::vector<uint8_t> fontData = PackageFiles::ReadBytes(fontFile);
	static const uint8_t spriteFontMagic[] = { 'D', 'X', 'T', 'K', 'f', 'o', 'n', 't', '_' };
	if (fontData.size() < sizeof(spriteFontMagic) || std::memcmp(fontData.data(), spriteFontMagic, sizeof(spriteFontMagic)) != 0)
	{
		OutputDebugStringW((L"TextRenderer: " + fontFile + L" is not a .spritefont made by MakeSpriteFont.\n").c_str());
		return;
	}

	auto device = deviceResources->GetD3DDevice();

	// One descriptor, for the font's texture.
	m_descriptors = std::make_unique<DescriptorHeap>(device, 1);

	ResourceUploadBatch upload(device);
	upload.Begin();

	m_font = std::make_unique<SpriteFont>(device, upload, fontData.data(), fontData.size(),
		m_descriptors->GetCpuHandle(0), m_descriptors->GetGpuHandle(0));

	RenderTargetState renderTarget(deviceResources->GetBackBufferFormat(), deviceResources->GetDepthBufferFormat());
	SpriteBatchPipelineStateDescription pipeline(renderTarget);
	m_batch = std::make_unique<SpriteBatch>(device, upload, pipeline);

	upload.End(deviceResources->GetCommandQueue()).wait();

	CreateWindowSizeDependentResources();
}

void TextRenderer::CreateWindowSizeDependentResources()
{
	if (m_batch != nullptr && m_deviceResources != nullptr)
	{
		m_batch->SetViewport(m_deviceResources->GetScreenViewport());
	}
}

void TextRenderer::ReleaseDeviceDependentResources()
{
	m_batch.reset();
	m_font.reset();
	m_descriptors.reset();
	m_deviceResources = nullptr;
	m_batchOpen = false;
}

void TextRenderer::Begin(ID3D12GraphicsCommandList* commandList, const RectF& view)
{
	if (!HasFont() || m_batchOpen)
	{
		return;
	}

	// Map the view onto the screen, the same way SpriteRenderer::Begin does.
	const D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
	const XMMATRIX transform =
		XMMatrixTranslation(-view.x, -view.y, 0.0f) *
		XMMatrixScaling(viewport.Width / view.width, viewport.Height / view.height, 1.0f);

	ID3D12DescriptorHeap* heaps[] = { m_descriptors->Heap() };
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);
	m_batch->Begin(commandList, SpriteSortMode_Deferred, transform);
	m_view = view;
	m_batchOpen = true;
}

void TextRenderer::Draw(const std::wstring& text, XMFLOAT2 position, XMFLOAT4 color, float scale, bool centred)
{
	if (!m_batchOpen || color.w <= 0.0f)
	{
		return;
	}

	XMFLOAT2 origin = { 0.0f, 0.0f };
	if (centred)
	{
		const XMVECTOR size = m_font->MeasureString(text.c_str());
		origin = { XMVectorGetX(size) * 0.5f, XMVectorGetY(size) * 0.5f };
	}

	// SpriteBatch blends like the sprite renderer: colors multiplied by their opacity.
	const float a = color.w > 1.0f ? 1.0f : color.w;
	const XMVECTOR premultiplied = XMVectorSet(color.x * a, color.y * a, color.z * a, a);

	m_font->DrawString(m_batch.get(), text.c_str(), position, premultiplied, 0.0f, origin, scale);
}

void TextRenderer::End()
{
	if (m_batchOpen)
	{
		m_batch->End();
		m_batchOpen = false;
	}
}

XMFLOAT2 TextRenderer::Measure(const std::wstring& text, float scale) const
{
	if (!HasFont())
	{
		return { 0.0f, 0.0f };
	}
	const XMVECTOR size = m_font->MeasureString(text.c_str());
	return { XMVectorGetX(size) * scale, XMVectorGetY(size) * scale };
}
