#pragma once

// A level's picture: a fixed stack of layers, drawn back to front, in the same order every
// frame:
//
//   background layers   1 or more, farthest first, parallax or stationary (see Background)
//   action layer        the level the player moves through: everything that plays, and decor
//   front layer         optional flair in front of the action, closer to the screen
//   indicator layer     damage numbers and other markers over the action
//   GUI layer           health and the rest of the HUD, fixed to the screen
//   map layer           the explored map over everything, only while it is open
//
// The background, action and front layers together are at most 12. All of them except the
// GUI and map layers are seen through the camera: a layer moving at 50% shows the part of its
// own space at half the camera's position, so it slides by at half speed.
//
// A layer draws whatever its functions draw, so the same scene holds art, placeholder shapes,
// the World or the HUD. Set them once, after building the scene.

#include "Background.h"
#include "Graphics.h"

#include <functional>
#include <vector>

namespace MechaMan
{
	struct SceneLayer
	{
		const char* name = "";
		float speed = 1.0f;				// how fast it scrolls: 1 moves with the action layer, 0 stays put
		bool fixedToScreen = false;		// drawn in screen units (c_view) whatever the camera does
		bool visible = true;

		// What the layer draws, given the view it is seen through. Each runs inside a batch of
		// its own: the sprites first, then the text on top of them.
		std::function<void(SpriteRenderer& sprites, const RectF& view)> drawSprites;
		std::function<void(TextRenderer& text, const RectF& view)> drawText;
	};

	// The scene's own stack, a sidekick to the GameStateStack: layers in the order they are
	// drawn, bottom first. The order is fixed once they are pushed.
	class LayerStack
	{
	public:
		size_t Push(SceneLayer layer)
		{
			m_layers.push_back(std::move(layer));
			return m_layers.size() - 1;
		}

		SceneLayer& operator[](size_t index) { return m_layers[index]; }
		const SceneLayer& operator[](size_t index) const { return m_layers[index]; }
		size_t Size() const { return m_layers.size(); }

		// The part of a layer's own space that fills the screen, given the action layer's view
		// (camera is its top-left corner). Snapped to whole units so pixel art scrolls crisply.
		static RectF ViewOf(const SceneLayer& layer, DirectX::XMFLOAT2 camera, DirectX::XMFLOAT2 viewSize);

		// Every visible layer, bottom to top.
		void Draw(Graphics& graphics, ID3D12GraphicsCommandList* commandList, DirectX::XMFLOAT2 camera, DirectX::XMFLOAT2 viewSize) const;

	private:
		std::vector<SceneLayer> m_layers;
	};

	class Scene
	{
	public:
		static constexpr int c_maxStageLayers = 12;		// background, action and front together

		// 'frontSpeedPercent': how fast the front layer moves compared with the action layer.
		// 100 moves with it; above 100 slides faster, so it feels closer to the screen.
		Scene(const Background& background, bool frontLayer, float frontSpeedPercent = 100.0f);

		DirectX::XMFLOAT2 camera = { 0.0f, 0.0f };			// top-left corner of the action layer's view, world units
		DirectX::XMFLOAT2 viewSize = { 320.0f, 180.0f };	// how much of the action layer fills the screen

		int BackgroundCount() const { return m_backgroundCount; }
		SceneLayer& BackgroundLayer(int index) { return m_layers[static_cast<size_t>(index)]; }	// 0 is the farthest
		SceneLayer& ActionLayer() { return m_layers[m_action]; }
		SceneLayer* FrontLayer() { return m_front >= 0 ? &m_layers[static_cast<size_t>(m_front)] : nullptr; }	// null when there is none
		SceneLayer& IndicatorLayer() { return m_layers[m_indicator]; }
		SceneLayer& GuiLayer() { return m_layers[m_gui]; }
		SceneLayer& MapLayer() { return m_layers[m_map]; }	// starts hidden

		// Centres the action layer's view on 'target' as far as the level's edges allow.
		void Follow(DirectX::XMFLOAT2 target, DirectX::XMFLOAT2 levelSize);

		void Draw(Graphics& graphics, ID3D12GraphicsCommandList* commandList) const
		{
			m_layers.Draw(graphics, commandList, camera, viewSize);
		}

	private:
		LayerStack	m_layers;
		int			m_backgroundCount;
		size_t		m_action;
		int			m_front;
		size_t		m_indicator;
		size_t		m_gui;
		size_t		m_map;
	};

	// Draws 'texture' side by side at its own size, all the way across the view, with its top
	// at 'y' in the layer's units: the usual way to fill a background layer from a strip of art.
	void DrawRepeatingX(SpriteRenderer& sprites, const Texture* texture, const RectF& view, float y, int layer = 0);
}
