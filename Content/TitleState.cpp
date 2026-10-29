#include "pch.h"
#include "TitleState.h"

#include "GameplayState.h"

#include "PackageFiles.h"

#include <cmath>

using namespace MechaMan;

TitleState::TitleState(GameStateStack& stack, Graphics& graphics, InputManager& input) :
	GameState(stack),
	m_graphics(graphics),
	m_input(input),
	m_loaded(false),
	m_seconds(0.0f)
{
}

void TitleState::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>&)
{
	// The splash screen loaded this already: GetTexture hands back the same texture.
	if (PackageFiles::Exists(L"Assets\\Logo.png"))
	{
		m_logo = m_graphics.sprites.GetTexture(L"Assets\\Title.png");
	}
	m_loaded = true;
}

void TitleState::ReleaseDeviceDependentResources()
{
	m_logo = nullptr;
	m_loaded = false;
}

void TitleState::Update(DX::StepTimer const& timer)
{
	m_seconds += static_cast<float>(timer.GetElapsedSeconds());

	// PRESS START: Enter, Space, Escape, or the controller's A or Menu button.
	if (m_input.WasPressed(Action::Start) || m_input.WasPressed(Action::Confirm))
	{
		// TODO: when there is a first level, Replace the title with it here instead. For now
		// the input test screen goes on top, and pops itself to come back here.
		m_stack.Push(std::make_unique<GameplayState>(m_stack, m_graphics, m_input));
	}
}

void TitleState::Render(ID3D12GraphicsCommandList* commandList)
{
	// Everything fades in out of black over half a second.
	const float fadeIn = m_seconds < 0.5f ? m_seconds / 0.5f : 1.0f;

	m_graphics.sprites.Begin(c_view);
	m_graphics.sprites.DrawRectangle(c_view, { 0.0f, 0.0f, 0.0f, 1.0f }, -1);
	if (m_logo != nullptr)
	{
		const RectF top = { c_view.x, c_view.y, c_view.width, c_view.height * 0.65f };
		m_graphics.sprites.DrawTexture(m_logo.get(), FitInside(m_logo->GetWidth(), m_logo->GetHeight(), top, 0.8f), fadeIn);
	}
	m_graphics.sprites.End(commandList);

	// PRESS START blinks once a second.
	const float blink = 0.5f + 0.5f * cosf(m_seconds * DirectX::XM_2PI);
	m_graphics.text.Begin(commandList);
	m_graphics.text.Draw(L"PRESS START", { c_view.x + c_view.width * 0.5f, c_view.y + c_view.height * 0.8f }, { 1.0f, 1.0f, 1.0f, fadeIn * blink }, 1.0f, true);
	m_graphics.text.End();
}
