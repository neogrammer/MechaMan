#include "pch.h"
#include "SplashState.h"
#include "../Common/DirectXHelper.h"
#include "../Common/DeviceResources.h"
#include "SpriteDemoState.h"

#include "PackageFiles.h"
#include <iostream>
#include "GameplayState.h"

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
	
	if (m_spriteVec.empty())
	{
		m_spriteVec.push_back(new CidSprite(CidSprite::m_spriteRenderer->LoadTexture(L"Assets\\StoreLogo.png")));
	}
	if (m_spriteVec[0]->GetTexture().get() != nullptr)
	{
		m_spriteVec[0]->Create(m_spriteVec[0]->GetTexture(), 300.f, 0.f);
	
	}

	
}

void SplashState::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	
	if (PackageFiles::Exists(L"Assets\\StoreLogo.png"))
	{
		if (m_spriteVec.empty())
		{
			m_spriteVec.push_back(new CidSprite{}); // CidSprite::m_spriteRenderer->LoadTexture(L"Assets\\StoreLogo.png")));
		}
		if (!m_spriteVec[0]->GetTexture())
		{
			m_spriteVec[0]->SetTexture( CidSprite::m_spriteRenderer->LoadTexture(L"Assets\\StoreLogo.png"));
			m_spriteVec[0]->Create(m_spriteVec[0]->GetTexture(), 300.f, 0.f);
			
		}
	}
	else
	{
		if (m_spriteVec.empty())
		{
			m_spriteVec.push_back(new CidSprite(CidSprite::m_spriteRenderer->LoadTexture(L"Assets\\StoreLogo.png")));
		}
		m_spriteVec[0]->GetTexture() = CreateBallTexture(*CidSprite::m_spriteRenderer);
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
	m_spriteVec[0]->SetTexture(nullptr);
}

bool SplashState::IsLoaded()
{
	return m_spriteVec[0]->GetTexture()!= nullptr;
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
		m_stack.Replace(std::make_unique<GameplayState>(m_stack));
	}
}

void SplashState::Render(ID3D12GraphicsCommandList* commandList)
{

	CidSprite::m_spriteRenderer->Begin();

	Sprite sprite2;
	sprite2.texture = m_spriteVec[0]->GetTexture().get();
	sprite2.source = { 0, 0, m_spriteVec[0]->GetEntity().size, m_spriteVec[0]->GetEntity().size};
	sprite2.position = DirectX::XMFLOAT2(300.f, 100.f);
	sprite2.flipX = false;
	sprite2.layer = 2;

	//m_cubeRenderer->Rotate(m_angle);
	CidSprite::m_spriteRenderer->Draw(sprite2);

	CidSprite::m_spriteRenderer->End(commandList);
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
