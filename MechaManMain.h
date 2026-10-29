#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\GameStateStack.h"
#include "Content\Graphics.h"
#include "Content\InputManager.h"

#include <mutex>

// Renders Direct3D content on the screen.
namespace MechaMan
{
	class MechaManMain
	{
	public:
		MechaManMain();
		void CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void Update();
		bool Render();

		void OnWindowSizeChanged();
		void OnSuspending();
		void OnResuming();
		void OnDeviceRemoved();

	private:
		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// The command list each frame is recorded on. Every state draws into it.
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

		// The sprite and text renderers every state draws with. Declared before the states
		// so that it outlives them: they hold textures that belong to it.
		Graphics m_graphics;

		// The keyboard and controllers, read once per update. Also declared before the states,
		// which keep a reference to it.
		InputManager m_input;

		// The game states. The top of the stack is the state the player is in.
		GameStateStack m_states;

		// Rendering loop timer.
		DX::StepTimer m_timer;

		// App.cpp calls OnSuspending from a background task while the game loop can still be
		// running. This keeps the two from using the states at the same time.
		std::mutex m_mutex;
	};
}
