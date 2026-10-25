#include "pch.h"
#include "SplashState.h"
#include "../Common/DirectXHelper.h"
#include "../Common/DeviceResources.h"
#include "SpriteDemoState.h"

#include "PackageFiles.h"
#include <cstring>
#include <iostream>
#include "GameplayState.h"
#include "ResourceUploadBatch.h"

using namespace MechaMan;

using namespace Windows::Foundation;
using namespace Windows::Storage;
extern const MechaMan::RectF MechaMan::c_view;
namespace
{
	// Index into the application state map.
	Platform::String^ TimeKey = "Elapsed";

	// A round, shaded ball made in code, for projects without the image the demo looks for.
	std::shared_ptr<Texture> CreateBallTexture(SpriteRenderer& sprites)
	{
		const UINT size = 32;
		std::vector<uint8_t> pixels(size * size * 4);

		for (UINT y = 0; y < size; y++)
		{
			for (UINT x = 0; x < size; x++)
			{
				const float dx = (static_cast<float>(x) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
				const float dy = (static_cast<float>(y) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
				const float distance = sqrtf(dx * dx + dy * dy);

				// Opaque inside the circle, clear outside, lighter towards the top-left.
				const float opacity = distance < 1.0f ? 1.0f : 0.0f;
				const float shade = 1.0f - 0.5f * sqrtf((dx + 0.4f) * (dx + 0.4f) + (dy + 0.4f) * (dy + 0.4f));
				const float brightness = (shade < 0.25f ? 0.25f : shade) * opacity;

				// The color bytes are multiplied by the opacity, as CreateTexture expects.
				uint8_t* pixel = &pixels[(y * size + x) * 4];
				pixel[0] = static_cast<uint8_t>(brightness * 255.0f);
				pixel[1] = static_cast<uint8_t>(brightness * 255.0f);
				pixel[2] = static_cast<uint8_t>(brightness * 255.0f);
				pixel[3] = static_cast<uint8_t>(opacity * 255.0f);
			}
		}

		return sprites.CreateTexture(size, size, pixels.data());
	}

}

SplashState::SplashState(GameStateStack& stack, SpriteRenderer& sprites) :
	GameState(stack),
	m_timeElapsed(0)
{
	LoadState();


	CidSprite::SetSpriteRenderer(std::addressof(sprites));
}

void SplashState::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	m_deviceResources = deviceResources;
	if (deviceResources == nullptr || CidSprite::m_spriteRenderer == nullptr)
	{
		// OutputDebugStringW can fail during app shutdown or certain thread conditions,
		// causing invalid parameter exceptions. Avoid calling it here.
		return;
	}

	LoadFonts(deviceResources);

	if (m_spriteVec.empty())
	{
		std::shared_ptr<Texture> texture;
		if (PackageFiles::Exists(L"Assets\\Logo.png"))
		{
			texture = CidSprite::m_spriteRenderer->LoadTexture(L"Assets\\Logo.png");
		}
		else
		{
			texture = CreateBallTexture(*CidSprite::m_spriteRenderer);
		}

		if (texture != nullptr)
		{
			m_spriteVec.push_back(new CidSprite(texture));
		}
	}
	if (m_spriteBatch != nullptr && m_deviceResources != nullptr)
	{
		m_viewport = m_deviceResources->GetScreenViewport();
		m_spriteBatch->SetViewport(m_viewport);
	}

	if (!m_spriteVec.empty() && m_spriteVec[0] != nullptr && m_spriteVec[0]->GetTexture() != nullptr)
	{
		m_spriteVec[0]->Create(m_spriteVec[0]->GetTexture(), 300.f, 0.f);
	}
}

void SplashState::CreateWindowSizeDependentResources()
{
	if (m_spriteBatch != nullptr && m_deviceResources != nullptr)
	{
		m_viewport = m_deviceResources->GetScreenViewport();
		m_spriteBatch->SetViewport(m_viewport);
	}
}

void SplashState::ReleaseDeviceDependentResources()
{
	// Font and SpriteBatch resources are tied to the old D3D device too.
	m_font.reset();
	m_spriteBatch.reset();
	m_resourceDescriptors.reset();
	m_deviceResources = nullptr;

	for (CidSprite* sprite : m_spriteVec)
	{
		if (sprite != nullptr)
		{
			sprite->SetTexture(nullptr);
			delete sprite;
		}
	}
	m_spriteVec.clear();
}

bool SplashState::IsLoaded()
{
	return !m_spriteVec.empty() &&
		m_spriteVec[0] != nullptr &&
		m_spriteVec[0]->GetTexture() != nullptr;
}

void SplashState::OnSuspending()
{
	try
	{
		SaveState();
	}
	catch (...)
	{
		// Silently ignore exceptions during suspension/shutdown
	}
}

// Game logic only. Nothing in here touches the GPU.
void SplashState::Update(DX::StepTimer const& timer)
{
	try
	{
		// Rotate the cube a small amount.
		m_timeElapsed += static_cast<float>(timer.GetElapsedSeconds());

		if (m_timeElapsed > 5.f)
		{
			m_stack.Replace(std::make_unique<SpriteDemoState>(m_stack));
		}
	}
	catch (...)
	{
		// Gracefully handle update errors
	}
}

void SplashState::Render(ID3D12GraphicsCommandList* commandList)
{
	if (!IsLoaded() || CidSprite::m_spriteRenderer == nullptr)
	{
		return;
	}
	//m_commandList = commandList;

	try
	{
		// Render sprites first
		CidSprite::m_spriteRenderer->Begin();
		Sprite sprite2;
		sprite2.texture = m_spriteVec[0]->GetTexture().get();
		sprite2.source = { 0, 0, m_spriteVec[0]->GetEntity().size, m_spriteVec[0]->GetEntity().size};
		sprite2.position = DirectX::XMFLOAT2(300.f, 100.f);
		sprite2.flipX = false;
		sprite2.layer = 2;

		CidSprite::m_spriteRenderer->Draw(sprite2);

		CidSprite::m_spriteRenderer->End(commandList);

		// Now render text if fonts are loaded
		if (m_resourceDescriptors != nullptr && m_spriteBatch != nullptr && m_font != nullptr)
		{
			ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap() };
			commandList->SetDescriptorHeaps(_countof(heaps), heaps);
			m_spriteBatch->Begin(commandList);
			m_font->DrawString(
				m_spriteBatch.get(),
				L"Blu Ballz Ink",
				DirectX::XMFLOAT2(m_viewport.Width / 2.f + (m_font->MeasureString("Blu Ballz Ink").m128_f32[3] - m_font->MeasureString("Blu Ballz Ink").m128_f32[0]) / 2.f, m_viewport.Height / 2.f - 18.f),
				DirectX::Colors::White
			);
			m_spriteBatch->End();
		}
		auto wid = m_viewport.Width;
		std::string widStr = std::to_string(wid) + " , ";

		auto hid = m_viewport.Height;
		std::string hidStr = std::to_string(hid) + "\n";
		std::string temp = widStr + hidStr;
		LPCSTR temp2 = temp.c_str();
		LPCSTR wid2 = widStr.c_str();
		LPCSTR hid2 = hidStr.c_str();
		
		OutputDebugStringA(temp2);
	}
	catch (const std::exception& e)
	{
		OutputDebugStringA("SplashState::Render exception: ");
		OutputDebugStringA(e.what());
		OutputDebugStringA("\n");
	}
	catch (...)
	{
		OutputDebugStringW(L"SplashState::Render: Unknown exception\n");
	}
}

// Saves the current state of the game.
void SplashState::SaveState()
{
	try
	{
		auto state = ApplicationData::Current->LocalSettings->Values;

		if (state->HasKey(TimeKey))
		{
			state->Remove(TimeKey);
		}

		state->Insert(TimeKey, PropertyValue::CreateSingle(m_timeElapsed));
	}
	catch (...)
	{
		// Silently ignore failures to save state. This can happen during app shutdown
		// or when ApplicationData access is unavailable.
	}
}

// Restores the previous state of the game.
void SplashState::LoadState()
{
	try
	{
		auto state = ApplicationData::Current->LocalSettings->Values;
		if (state->HasKey(TimeKey))
		{
			m_timeElapsed = safe_cast<IPropertyValue^>(state->Lookup(TimeKey))->GetSingle();
			state->Remove(TimeKey);
		}
	}
	catch (...)
	{
		// Silently ignore failures to load state. This can happen during app initialization
		// or when ApplicationData access is unavailable. Use default m_timeElapsed value.
	}
}

void MechaMan::SplashState::LoadFonts(std::shared_ptr<DX::DeviceResources> deviceResources)
{
	try
	{
		static const uint8_t spriteFontMagic[] = { 'D', 'X', 'T', 'K', 'f', 'o', 'n', 't', '_' };
		const std::wstring fontPath = L"fonts\\myfile.spritefont";

		// Font uploads are optional. The sprite/game loop must remain usable when a
		// packaged spritefont is missing or incompatible with the installed DirectXTK.
		// Do not recreate these GPU objects from the window-size callback.
		if (m_font != nullptr && m_spriteBatch != nullptr && m_resourceDescriptors != nullptr)
		{
			return;
		}
		if (deviceResources == nullptr)
		{
			return;
		}

		auto device = deviceResources->GetD3DDevice();
		auto commandQueue = deviceResources->GetCommandQueue();
		if (device == nullptr || commandQueue == nullptr)
		{
			return;
		}

		enum Descriptors
		{
			MyFont,
			Count
		};

		// Try to load font file
		if (!PackageFiles::Exists(fontPath))
		{
			return;
		}

		std::vector<uint8_t> fontData = PackageFiles::ReadBytes(fontPath);
		if (fontData.empty() || fontData.size() < sizeof(spriteFontMagic) ||
			std::memcmp(fontData.data(), spriteFontMagic, sizeof(spriteFontMagic)) != 0)
		{
			return;
		}

		// Create descriptor heap
		auto resourceDescriptors = std::make_unique<DirectX::DescriptorHeap>(
			device,
			Descriptors::Count
		);

		// Create SpriteFont
		DirectX::ResourceUploadBatch resourceUpload(device);
		resourceUpload.Begin();

		auto font = std::make_unique<DirectX::SpriteFont>(
			device,
			resourceUpload,
			fontData.data(),
			fontData.size(),
			resourceDescriptors->GetCpuHandle(Descriptors::MyFont),
			resourceDescriptors->GetGpuHandle(Descriptors::MyFont)
		);

		auto uploadFontTask = resourceUpload.End(commandQueue);
		if (uploadFontTask.valid())
		{
			uploadFontTask.wait();
		}

		// Create SpriteBatch
		DirectX::RenderTargetState rtState(
			deviceResources->GetBackBufferFormat(),
			deviceResources->GetDepthBufferFormat()
		);

		DirectX::SpriteBatchPipelineStateDescription pd(rtState);

		DirectX::ResourceUploadBatch spriteBatchUpload(device);
		spriteBatchUpload.Begin();

		auto spriteBatch = std::make_unique<DirectX::SpriteBatch>(
			device,
			spriteBatchUpload,
			pd
		);

		auto uploadBatchTask = spriteBatchUpload.End(commandQueue);
		if (uploadBatchTask.valid())
		{
			uploadBatchTask.wait();
		}

		// All resources created successfully, assign them
		m_resourceDescriptors = std::move(resourceDescriptors);
		m_font = std::move(font);
		m_spriteBatch = std::move(spriteBatch);
	}
	catch (const std::exception& e)
	{
		std::string msg = "LoadFonts error: ";
		msg += e.what();
		msg += "\n";
		OutputDebugStringA(msg.c_str());
	}
	catch (...)
	{
		OutputDebugStringW(L"LoadFonts: Unknown exception\n");
	}
}

