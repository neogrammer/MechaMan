#include "pch.h"
#include "GameStateStack.h"

using namespace MechaMan;

GameStateStack::GameStateStack()
{
}

GameStateStack::~GameStateStack()
{
	// Destroy the states from the top down, the same order they would be popped in.
	m_pending.clear();
	while (!m_states.empty())
	{
		m_states.pop_back();
	}
}

void GameStateStack::Push(std::unique_ptr<GameState> state)
{
	if (state != nullptr)
	{
		m_pending.push_back(Change{ ChangeType::Push, std::move(state) });
	}
}

void GameStateStack::Pop()
{
	m_pending.push_back(Change{ ChangeType::Pop, nullptr });
}

void GameStateStack::Replace(std::unique_ptr<GameState> state)
{
	Pop();
	Push(std::move(state));
}

// Applies the queued Push and Pop requests, in the order they were made.
void GameStateStack::ApplyPendingChanges()
{
	if (m_pending.empty())
	{
		return;
	}

	// OnEnter and OnExit may queue more changes. Those wait for the next update.
	std::vector<Change> changes;
	changes.swap(m_pending);

	bool gpuIdle = false;

	for (auto& change : changes)
	{
		if (change.type == ChangeType::Push)
		{
			if (m_deviceResources != nullptr)
			{
				change.state->CreateDeviceDependentResources(m_deviceResources);
				change.state->CreateWindowSizeDependentResources();
			}

			m_states.push_back(std::move(change.state));
			m_states.back()->OnEnter();
		}
		else if (!m_states.empty())
		{
			m_states.back()->OnExit();

			// Frames already sent to the GPU may still be drawing with this state's
			// resources. Let them finish before the state, and those resources, go away.
			if (!gpuIdle && m_deviceResources != nullptr && !m_deviceResources->IsDeviceRemoved())
			{
				m_deviceResources->WaitForGpu();
				gpuIdle = true;
			}

			m_states.pop_back();
		}
	}
}

// Index of the lowest state that gets drawn: the highest opaque one, or the bottom of the stack.
size_t GameStateStack::FirstVisibleIndex() const
{
	size_t index = m_states.size();
	while (index > 0)
	{
		index--;
		if (m_states[index]->IsOpaque())
		{
			break;
		}
	}
	return index;
}

// Called at startup, and again after the device has been lost and re-created.
void GameStateStack::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	m_deviceResources = deviceResources;

	for (auto& state : m_states)
	{
		state->CreateDeviceDependentResources(deviceResources);
		state->CreateWindowSizeDependentResources();
	}
}

void GameStateStack::CreateWindowSizeDependentResources()
{
	if (m_deviceResources == nullptr)
	{
		return;
	}

	for (auto& state : m_states)
	{
		state->CreateWindowSizeDependentResources();
	}
}

// The device is gone. Every state drops its GPU objects and keeps its game data.
void GameStateStack::ReleaseDeviceDependentResources()
{
	for (auto& state : m_states)
	{
		state->ReleaseDeviceDependentResources();
	}

	m_deviceResources = nullptr;
}

void GameStateStack::OnSuspending()
{
	for (auto& state : m_states)
	{
		state->OnSuspending();
	}
}

void GameStateStack::OnResuming()
{
	for (auto& state : m_states)
	{
		state->OnResuming();
	}
}

// Updates from the top down, stopping after the first state that pauses the ones below it.
void GameStateStack::Update(DX::StepTimer const& timer)
{
	ApplyPendingChanges();

	for (auto it = m_states.rbegin(); it != m_states.rend(); ++it)
	{
		GameState& state = **it;

		if (state.IsLoaded())
		{
			state.Update(timer);
		}

		if (state.PausesBelow())
		{
			break;
		}
	}
}

bool GameStateStack::IsReadyToRender() const
{
	for (size_t i = FirstVisibleIndex(); i < m_states.size(); i++)
	{
		if (!m_states[i]->IsLoaded())
		{
			return false;
		}
	}
	return true;
}

// Draws the visible states, lowest first, so each one draws over the ones below it.
void GameStateStack::Render(ID3D12GraphicsCommandList* commandList)
{
	for (size_t i = FirstVisibleIndex(); i < m_states.size(); i++)
	{
		if (m_states[i]->IsLoaded())
		{
			m_states[i]->Render(commandList);
		}
	}
}
