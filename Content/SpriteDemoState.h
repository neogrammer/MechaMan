#pragma once

#include "GameStateStack.h"
#include "SpriteRenderer.h"
#include "Entity.h"
#include <vector>

namespace MechaMan
{
	// Shows the sprite renderer at work: a few hundred things drift around the screen, each
	// one moved in Update and drawn in Render. It sits on top of the gameplay state without
	// hiding or pausing it, so the cube keeps turning underneath. Take it out again by
	// removing its Push from the MechaManMain constructor.
	class SpriteDemoState : public GameState
	{
	public:
		explicit SpriteDemoState(GameStateStack& stack);
		SpriteDemoState(const SpriteDemoState&) = delete;
		SpriteDemoState& operator=(const SpriteDemoState&) = delete;

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources) override;
		void CreateWindowSizeDependentResources() override;
		void ReleaseDeviceDependentResources() override;
		bool IsLoaded() override;

		void Update(DX::StepTimer const& timer) override;
		void Render(ID3D12GraphicsCommandList* commandList) override;

		bool IsOpaque() const override { return false; }
		bool PausesBelow() const override { return false; }

	private:
		// One thing on screen. This is game data: nothing in it belongs to the GPU.


	private:

		// GPU objects. Created and released along with the device.
		std::shared_ptr<Texture> m_badge;

		// Game data. Stays put when the device is lost.
		std::vector<Entity>	m_entities;
		double				m_secondsSinceReport;
	};
}
