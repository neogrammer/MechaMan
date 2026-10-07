#pragma once

#include "GameStateStack.h"
#include "Sample3DSceneRenderer.h"

namespace MechaMan
{
	// The state the game starts in. For now it is the template's spinning cube:
	// the state owns the game data (the angle) and a renderer that draws it.
	class GameplayState : public GameState
	{
	public:
		explicit GameplayState(GameStateStack& stack);

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
		std::shared_ptr<Sample3DSceneRenderer> m_cubeRenderer;

		// Game data. Stays put when the device is lost.
		float	m_radiansPerSecond;
		float	m_angle;
	};
}
