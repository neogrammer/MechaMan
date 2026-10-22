#pragma once

#include "GameStateStack.h"

#include "SpriteRenderer.h"
#include "Entity.h"

#include "CidSprite.h"
#include <memory>
#include "../Common/DeviceResources.h"

#include "SpriteFont.h"
#include "ResourceUploadBatch.h"
#include "DescriptorHeap.h"

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
		bool IsLoaded() override;

		void OnSuspending() override;

		void Update(DX::StepTimer const& timer) override;
		void Render(ID3D12GraphicsCommandList* commandList) override;


		void LoadFonts(std::shared_ptr<DX::DeviceResources> deviceResources);
	private:
		void SaveState();
		void LoadState();


	

	private:

		// font
		std::unique_ptr<DirectX::SpriteFont> m_font;
		std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;

		// GPU objects. Created and released along with the device.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// For your command list
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

		// For your sprite batch
		std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;
		D3D12_VIEWPORT m_viewport{};

		std::vector<CidSprite*> m_spriteVec;
		// Game data. Stays put when the device is lost.
		float	m_timeElapsed;
		//std::vector<Entity>	m_entities;
	};
}
