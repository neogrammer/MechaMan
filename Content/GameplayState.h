#pragma once

#include "GameStateStack.h"
#include "Graphics.h"
#include "InputManager.h"
#include "../Tilemaps/Tilemap.h"

namespace MechaMan
{
	// The state the game starts in. For now it is the template's spinning cube:
	// the state owns the game data (the angle) and a renderer that draws it.
	class GameplayState : public GameState
	{
	public:



		explicit GameplayState(GameStateStack& stack, Graphics& graphics, InputManager& input);

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources) override;
		void CreateWindowSizeDependentResources() override {}
		void ReleaseDeviceDependentResources() override;
		bool IsLoaded() override { return m_loaded; }

		void Update(DX::StepTimer const& timer) override;
		void Render(ID3D12GraphicsCommandList* commandList) override;

	private:
		Graphics& m_graphics;
		InputManager& m_input;

		// GPU objects.
		Tilemap m_tmap{};
		std::shared_ptr<Texture>	m_tileset;
		

		bool						m_loaded;

	};
}


