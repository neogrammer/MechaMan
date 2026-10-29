#pragma once

#include "GameStateStack.h"
#include "Graphics.h"
#include "InputManager.h"

#include <array>

namespace MechaMan
{
	// A screen for trying out the controls. Every action is listed with a light that is on
	// while it is held and a count of how many times it has been pressed, and a square moves
	// around with the arrows, WASD, the D-pad or the left stick. Holding Back for a second
	// goes back to the title screen.
	//
	// The title screen opens it on PRESS START until there is a first level. Like
	// SpriteDemoState it is only here to try things out: once the first level exists, take
	// the Push out of TitleState.cpp and these two files can go.
	class InputTestState : public GameState
	{
	public:
		InputTestState(GameStateStack& stack, Graphics& graphics, InputManager& input);

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources) override;
		void CreateWindowSizeDependentResources() override {}
		void ReleaseDeviceDependentResources() override;
		bool IsLoaded() override { return m_loaded; }

		void Update(DX::StepTimer const& timer) override;
		void Render(ID3D12GraphicsCommandList* commandList) override;

	private:
		Graphics&		m_graphics;
		InputManager&	m_input;

		// Everything this screen draws with belongs to Graphics, so there is nothing of its
		// own to load. This follows the device being lost and re-created, like the others.
		bool			m_loaded;

		// Game data.
		std::array<unsigned int, c_actionCount>	m_presses;	// how many times each action has been pressed
		DirectX::XMFLOAT2						m_square;	// the centre of the square, in view units
		float									m_backHeld;	// how long Back has been held, in seconds
		bool									m_leaving;
	};
}
