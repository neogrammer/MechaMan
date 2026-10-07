#pragma once

#include "GameStateStack.h"

#include "SpriteRenderer.h"
#include "Entity.h"

namespace MechaMan
{
	// The state the game starts in. For now it is the template's spinning cube:
	// the state owns the game data (the angle) and a renderer that draws it.
	class SplashState : public GameState
	{
	public:
		explicit SplashState(GameStateStack& stack, SpriteRenderer& sprites);

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources) override;
		void CreateWindowSizeDependentResources() override;
		void ReleaseDeviceDependentResources() override;
		bool IsLoaded() const override;

		void OnSuspending() override;

		void Update(DX::StepTimer const& timer) override;
		void Render(ID3D12GraphicsCommandList* commandList) override;

	private:
		void SaveState();
		void LoadState();

	private:
		// GPU objects. Created and released along with the device.
		SpriteRenderer& m_spriteRenderer;
		std::shared_ptr<Texture> m_badge;

		// Game data. Stays put when the device is lost.
		float	m_timeElapsed;
		std::vector<Entity>	m_entities;
	};
}
