#pragma once

#include "DamageNumbers.h"
#include "ExplorationMap.h"
#include "GameStateStack.h"
#include "Graphics.h"
#include "InputManager.h"
#include "Scene.h"
#include "SpriteSheet.h"
#include "Stage.h"
#include "World.h"

#include <memory>

namespace MechaMan
{
	// The intro stage: the highway of Assets\backgrounds\intro.png from end to end, with the
	// ground, checkpoints and enemies of Assets\cfg\intro_stage.cfg, drawn as a Scene. The city
	// at dusk is the background layer, sliding by at 30%; the highway itself is drawn in the
	// action layer behind the World. Then damage numbers, the health bar and the map. Each
	// layer's drawing is set in SetUpLayers.
	//
	// Dying (or falling into a pit) starts the stage again from the last checkpoint reached,
	// with every enemy back. Back returns to the title screen. Start shows the hurt and hit
	// boxes and the stage's solids. Map opens the map, where the directions look around and
	// Map or Back closes it again.
	class ActorDemoState : public GameState
	{
	public:
		ActorDemoState(GameStateStack& stack, Graphics& graphics, InputManager& input);

		void CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources) override;
		void CreateWindowSizeDependentResources() override {}
		void ReleaseDeviceDependentResources() override;
		bool IsLoaded() override { return m_loaded; }

		void Update(DX::StepTimer const& timer) override;
		void Render(ID3D12GraphicsCommandList* commandList) override;

	private:
		void SetUpLayers();
		void StartWorld();		// a fresh world: the player at the last checkpoint reached, every enemy back

		Graphics&				m_graphics;
		InputManager&			m_input;

		// The stage art, created and released with the device.
		std::shared_ptr<Texture>	m_stageArt;

		// Declared before the world, which points into both.
		Stage					m_stage;
		SpriteLibrary			m_sheets;
		std::unique_ptr<World>	m_world;		// null when the stage or Alucard's sheet did not load

		Scene					m_scene;
		ExplorationMap			m_map;
		DamageNumbers			m_numbers;

		size_t					m_checkpoint;	// the furthest one reached
		float					m_deadFor;		// seconds since the player died
		int						m_playerMaxHealth;
		float					m_seconds;
		bool					m_loaded;
		bool					m_debugBoxes;
	};
}
