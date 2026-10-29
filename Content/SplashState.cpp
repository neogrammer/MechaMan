#include "pch.h"
#include "SplashState.h"

#include "PackageFiles.h"
#include "TitleState.h"

using namespace MechaMan;

namespace
{
	// The timeline, in seconds.
	const float c_fadeIn = 1.0f;	// black to logo
	const float c_hold = 2.0f;		// logo fully visible
	const float c_fadeOut = 1.0f;	// logo to black
	const float c_black = 0.3f;		// a moment of black before the title screen

	const wchar_t* c_logoFile = L"Assets\\Logo.png";
	const wchar_t* c_credit = L"Blu Ballz Ink";
}

SplashState::SplashState(GameStateStack& stack, Graphics& graphics, InputManager& input) :
	GameState(stack),
	m_graphics(graphics),
	m_input(input),
	m_loaded(false),
	m_seconds(0.0f),
	m_finished(false)
{
}

void SplashState::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>&)
{
	// A missing logo should not stop the game: the splash just shows black.
	if (PackageFiles::Exists(c_logoFile))
	{
		m_logo = m_graphics.sprites.GetTexture(c_logoFile);
	}
	else
	{
		OutputDebugStringW(L"SplashState: Assets\\Logo.png is not in the package. Is it in the project with Content set to Yes?\n");
	}
	m_loaded = true;
}

void SplashState::ReleaseDeviceDependentResources()
{
	m_logo = nullptr;
	m_loaded = false;
}

float SplashState::Opacity() const
{
	if (m_seconds < c_fadeIn)
	{
		return m_seconds / c_fadeIn;
	}
	if (m_seconds < c_fadeIn + c_hold)
	{
		return 1.0f;
	}
	if (m_seconds < c_fadeIn + c_hold + c_fadeOut)
	{
		return 1.0f - (m_seconds - c_fadeIn - c_hold) / c_fadeOut;
	}
	return 0.0f;
}

void SplashState::Update(DX::StepTimer const& timer)
{
	m_seconds += static_cast<float>(timer.GetElapsedSeconds());

	// Start or Confirm skips the rest of the splash.
	const bool skipped = m_input.WasPressed(Action::Start) || m_input.WasPressed(Action::Confirm);

	// Once is enough: the Replace only happens at the start of the next update.
	if (!m_finished && (skipped || m_seconds >= c_fadeIn + c_hold + c_fadeOut + c_black))
	{
		m_finished = true;
		m_stack.Replace(std::make_unique<TitleState>(m_stack, m_graphics, m_input));
	}
}

void SplashState::Render(ID3D12GraphicsCommandList* commandList)
{
	const float opacity = Opacity();

	// Fading the logo's opacity over a black background is the same as fading it to black.
	m_graphics.sprites.Begin(c_view);
	m_graphics.sprites.DrawRectangle(c_view, { 0.0f, 0.0f, 0.0f, 1.0f }, -1);
	if (m_logo != nullptr)
	{
		// As big as fits in 70% of the screen, centred, a little above the middle.
		RectF area = c_view;
		area.height *= 0.85f;
		m_graphics.sprites.DrawTexture(m_logo.get(), FitInside(m_logo->GetWidth(), m_logo->GetHeight(), area, 0.7f), opacity);
	}
	m_graphics.sprites.End(commandList);

	// The credit line under it, fading with it.
	m_graphics.text.Begin(commandList);
	m_graphics.text.Draw(c_credit, { c_view.x + c_view.width * 0.5f, c_view.y + c_view.height * 0.88f }, { 1.0f, 1.0f, 1.0f, opacity }, 1.0f, true);
	m_graphics.text.End();
}
