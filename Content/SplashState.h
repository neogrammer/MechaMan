#pragma once

#include "GameStateStack.h"
#include "Graphics.h"
#include "InputManager.h"

namespace MechaMan
{
	// The first thing the player sees: the logo fades in out of black, stays a moment,
	// fades out again, and the title screen takes over. Start or Confirm skips straight
	// to the title screen.
	class SplashState : public GameState
	{
	public:
		SplashState(GameStateStack& stack, Graphics& graphics, InputManager& input);

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources) override;
		void CreateWindowSizeDependentResources() override {}
		void ReleaseDeviceDependentResources() override;
		bool IsLoaded() override { return m_loaded; }

		void Update(DX::StepTimer const& timer) override;
		void Render(ID3D12GraphicsCommandList* commandList) override;

	private:
		// How opaque the logo is right now, from 0 to 1.
		float Opacity() const;

		Graphics&					m_graphics;
		InputManager&				m_input;

		// GPU objects. Created and released along with the device.
		std::shared_ptr<Texture>	m_logo;
		bool						m_loaded;

		// Game data.
		float						m_seconds;		// since the splash started
		bool						m_finished;
	};
}
