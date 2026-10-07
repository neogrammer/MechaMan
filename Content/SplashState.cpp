#include "pch.h"
#include "SplashState.h"
#include "../Common/DirectXHelper.h"
#include "../Common/DeviceResources.h"
#include "SpriteDemoState.h"

#include "PackageFiles.h"
#include <iostream>


using namespace MechaMan;

using namespace Windows::Foundation;
using namespace Windows::Storage;

namespace
{
	// Index into the application state map.
	Platform::String^ TimeKey = "Elapsed";
	const RectF c_view = { 0.0f, 0.0f, 1920.0f, 1080.0f };

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
	m_spriteRenderer(sprites),
	m_timeElapsed(0)
{
	LoadState();

	m_entities.resize(1);

	Entity& entity = m_entities[0];

	entity.isBadge = true;
	entity.size = { 50.f };
	entity.position = { c_view.width / 2.f, c_view.height / 2.f};

	const float direction = DirectX::XM_2PI * 0.7f;
	const float speed = 60.0f + 240.0f;
	entity.velocity = { 0.f,0.f };

	entity.angle = DirectX::XM_2PI * 0.4f;
	entity.spin = 0.f;

	if (entity.isBadge)
	{
		entity.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	}
	else
	{
		// A bright color, partly see-through.
		entity.color = { 0.3f + 0.7f,  0.3f + 0.1f ,  0.3f + 0.4f, 0.6f };
	}

}

void SplashState::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	
	if (PackageFiles::Exists(L"Assets\\StoreLogo.png"))
	{
		m_badge = m_spriteRenderer.LoadTexture(L"Assets\\StoreLogo.png");
	}
	else
	{
		m_badge = CreateBallTexture(m_spriteRenderer);
	}

		
	
}

void SplashState::CreateWindowSizeDependentResources()
{
	//if (m_spriteRenderer != nullptr)
	//{
	//	m_spriteRenderer->CreateWindowSizeDependentResources();
	//}
}

void SplashState::ReleaseDeviceDependentResources()
{
	m_badge = nullptr;
}

bool SplashState::IsLoaded() const
{
	return m_badge != nullptr;
}

void SplashState::OnSuspending()
{
	SaveState();
}

// Game logic only. Nothing in here touches the GPU.
void SplashState::Update(DX::StepTimer const& timer)
{
	// Rotate the cube a small amount.
	m_timeElapsed += static_cast<float>(timer.GetElapsedSeconds());

	if (m_timeElapsed > 20.f)
	{
		std::cout << "Yup" << std::endl;
		m_stack.Replace(std::make_unique<SpriteDemoState>(m_stack, m_spriteRenderer));
	}
}

void SplashState::Render(ID3D12GraphicsCommandList* commandList)
{

	m_spriteRenderer.Begin();

	Sprite sprite;
	sprite.texture = m_entities[0].isBadge ? m_badge.get() : CreateBallTexture(m_spriteRenderer).get();
	sprite.source = { 0, 0, 50, 50 };
	sprite.position = DirectX::XMFLOAT2(100.f, 100.f);
	sprite.flipX = false;
	sprite.layer = 2;
	//m_cubeRenderer->Rotate(m_angle);
	m_spriteRenderer.Draw(sprite);

	m_spriteRenderer.End(commandList);
}

// Saves the current state of the game.
void SplashState::SaveState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;

	if (state->HasKey(TimeKey))
	{
		state->Remove(TimeKey);
	}

	state->Insert(TimeKey, PropertyValue::CreateSingle(m_timeElapsed));
}

// Restores the previous state of the game.
void SplashState::LoadState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;
	if (state->HasKey(TimeKey))
	{
		m_timeElapsed = safe_cast<IPropertyValue^>(state->Lookup(TimeKey))->GetSingle();
		state->Remove(TimeKey);
	}
}
