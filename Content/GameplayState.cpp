#include "pch.h"
#include "GameplayState.h"

using namespace MechaMan;

using namespace Windows::Foundation;
using namespace Windows::Storage;

namespace
{
	// Index into the application state map.
	Platform::String^ AngleKey = "Angle";
}

GameplayState::GameplayState(GameStateStack& stack) :
	GameState(stack),
	m_radiansPerSecond(DirectX::XM_PIDIV4),	// rotate 45 degrees per second
	m_angle(0)
{
	LoadState();
}

void GameplayState::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	m_cubeRenderer = Sample3DSceneRenderer::Create(deviceResources);
}

void GameplayState::CreateWindowSizeDependentResources()
{
	if (m_cubeRenderer != nullptr)
	{
		m_cubeRenderer->CreateWindowSizeDependentResources();
	}
}

void GameplayState::ReleaseDeviceDependentResources()
{
	m_cubeRenderer = nullptr;
}

bool GameplayState::IsLoaded()
{
	return m_cubeRenderer != nullptr && m_cubeRenderer->IsLoaded();
}

void GameplayState::OnSuspending()
{
	SaveState();
}

// Game logic only. Nothing in here touches the GPU.
void GameplayState::Update(DX::StepTimer const& timer)
{
	// Rotate the cube a small amount.
	m_angle += static_cast<float>(timer.GetElapsedSeconds()) * m_radiansPerSecond;
}

void GameplayState::Render(ID3D12GraphicsCommandList* commandList)
{
	m_cubeRenderer->Rotate(m_angle);
	m_cubeRenderer->Render(commandList);
}

// Saves the current state of the game.
void GameplayState::SaveState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;

	if (state->HasKey(AngleKey))
	{
		state->Remove(AngleKey);
	}

	state->Insert(AngleKey, PropertyValue::CreateSingle(m_angle));
}

// Restores the previous state of the game.
void GameplayState::LoadState()
{
	auto state = ApplicationData::Current->LocalSettings->Values;
	if (state->HasKey(AngleKey))
	{
		m_angle = safe_cast<IPropertyValue^>(state->Lookup(AngleKey))->GetSingle();
		state->Remove(AngleKey);
	}
}
