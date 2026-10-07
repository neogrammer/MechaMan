#pragma once

#include "GameState.h"

#include <memory>
#include <vector>

namespace MechaMan
{
	// Owns the game states. The top of the stack is the state the player is in;
	// the states under it are still alive and can keep drawing or updating.
	class GameStateStack
	{
	public:
		GameStateStack();
		~GameStateStack();

		// State changes. These only queue a request, which is applied at the start of the
		// next update. That makes it safe for a state to pop itself from its own Update.
		void Push(std::unique_ptr<GameState> state);
		void Pop();
		void Replace(std::unique_ptr<GameState> state);	// Pop, then Push

		bool IsEmpty() const { return m_states.empty(); }

		// GPU lifetime.
		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();

		// App lifetime.
		void OnSuspending();
		void OnResuming();

		// Per frame.
		void Update(DX::StepTimer const& timer);
		bool IsReadyToRender() const;	// false while a state that would be drawn is still loading
		void Render(ID3D12GraphicsCommandList* commandList);

		GameState* Top() const { return m_states.empty() ? nullptr : m_states.back().get(); }	

	private:
		enum class ChangeType
		{
			Push,
			Pop
		};

		struct Change
		{
			ChangeType type;
			std::unique_ptr<GameState> state;	// only set for a Push
		};

		void ApplyPendingChanges();
		size_t FirstVisibleIndex() const;

		// Cached pointer to device resources. Null while the device is lost.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// The states, bottom first. The last one is the top of the stack.
		std::vector<std::unique_ptr<GameState>> m_states;

		// Changes requested since the last update.
		std::vector<Change> m_pending;
	};
}
