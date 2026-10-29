#pragma once

#include "GameStateStack.h"
#include "Graphics.h"
#include "InputManager.h"

namespace MechaMan
{
	// The title screen: the logo at the top and a blinking PRESS START. Start or Confirm
	// opens the input test screen for now; it will open the first level once there is one.
	class TitleState : public GameState
	{
	public:
		TitleState(GameStateStack& stack, Graphics& graphics, InputManager& input);

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources) override;
		void CreateWindowSizeDependentResources() override {}
		void ReleaseDeviceDependentResources() override;
		bool IsLoaded() override { return m_loaded; }

		void Update(DX::StepTimer const& timer) override;
		void Render(ID3D12GraphicsCommandList* commandList) override;

	private:
		Graphics&					m_graphics;
		InputManager&				m_input;

		// GPU objects.
		std::shared_ptr<Texture>	m_logo;
		bool						m_loaded;

		// Game data.
		float						m_seconds;
	};
}
