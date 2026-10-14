#pragma once

#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"

#include <memory>

namespace MechaMan
{
	class GameStateStack;

	// One screen or mode of the game: title screen, gameplay, pause menu and so on.
	// States live on a GameStateStack, which calls the functions below.
	class GameState
	{
	public:
		explicit GameState(GameStateStack& stack) : m_stack(stack) {}
		virtual ~GameState() = default;

		GameState(const GameState&) = delete;
		GameState& operator=(const GameState&) = delete;

		// GPU lifetime. The device can be lost and re-created while the game runs, so these
		// can be called more than once and the state's game data has to survive in between.
		virtual void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources) = 0;
		virtual void CreateWindowSizeDependentResources() = 0;
		virtual void ReleaseDeviceDependentResources() = 0;	// drop every GPU object, keep the game data

		// True once the state's GPU resources are ready to draw with.
		// The stack does not call Update or Render before that.
		virtual bool IsLoaded() = 0;

		// Stack lifetime.
		virtual void OnEnter() {}	// just pushed onto the stack
		virtual void OnExit() {}	// about to be taken off the stack and destroyed

		// App lifetime. OnSuspending is called from a background thread; the game loop is held
		// still while it runs, so it can read the state's data safely.
		virtual void OnSuspending() {}	// save whatever must survive the app being terminated
		virtual void OnResuming() {}

		// Per frame. Update changes game data only. Render draws that data into the frame's
		// command list; the frame is already cleared and its render targets are already set.
		virtual void Update(DX::StepTimer const& timer) = 0;
		virtual void Render(ID3D12GraphicsCommandList* commandList) = 0;

		// What this state does to the states underneath it. Both apply from the moment the
		// state is pushed, even while it is still loading.
		virtual bool IsOpaque() const { return true; }		// false: the states below are drawn first
		virtual bool PausesBelow() const { return true; }	// false: the states below keep updating

	protected:
		// The stack this state is on. Use it to change state: m_stack.Push, Pop or Replace.
		GameStateStack& m_stack;
	};
}
