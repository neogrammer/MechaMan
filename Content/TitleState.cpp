#include "pch.h"
#include "TitleState.h"

#include "ActorDemoState.h"
#include "GameplayState.h"

#include "PackageFiles.h"

#include <cmath>

using namespace MechaMan;

TitleState::TitleState(GameStateStack& stack, Graphics& graphics, InputManager& input) :
	GameState(stack),
	m_graphics(graphics),
	m_input(input),
	m_loaded(false),
	m_seconds(0.0f),
	m_logo(nullptr)
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

	// Back quits the game: Escape or Backspace, or the controller's B. Escape is also one of
	// the Start keys, so this has to be checked first, or Escape would start the game too.
	if (m_input.WasPressed(Action::Back))
	{
		Windows::ApplicationModel::Core::CoreApplication::Exit();
		return;
	}

	// PRESS START: Enter or Space, or the controller's A or Menu button.
	if (m_input.WasPressed(Action::Start) || m_input.WasPressed(Action::Confirm))
	{
		// TODO: when there is a first level, Replace the title with it here instead. For now
		// the Alucard demo goes on top, and pops itself (Back) to come back here. Your tile
		// map is still in GameplayState: put GameplayState back here to see it.
		m_stack.Push(std::make_unique<ActorDemoState>(m_stack, m_graphics, m_input));
	}
}

void TitleState::Render(ID3D12GraphicsCommandList* commandList)
{
	// Everything fades in out of black over half a second.
	const float fadeIn = m_seconds < 0.5f ? m_seconds / 0.5f : 1.0f;

	m_graphics.sprites.Begin(c_view);
	//m_graphics.sprites.DrawRectangle(c_view, { 0.0f, 0.0f, 0.0f, 1.0f }, -1);
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
