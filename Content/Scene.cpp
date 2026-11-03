#include "pch.h"
#include "Scene.h"

#include <cmath>

using namespace MechaMan;
using namespace DirectX;

namespace
{
	SceneLayer MakeLayer(const char* name, float speed, bool fixedToScreen = false)
	{
		SceneLayer layer;
		layer.name = name;
		layer.speed = speed;
		layer.fixedToScreen = fixedToScreen;
		return layer;
	}

	float Clamp(float value, float low, float high)
	{
		return value < low ? low : (value > high ? high : value);
	}
}

RectF LayerStack::ViewOf(const SceneLayer& layer, XMFLOAT2 camera, XMFLOAT2 viewSize)
{
	if (layer.fixedToScreen)
	{
		return c_view;
	}
	return { std::floor(camera.x * layer.speed + 0.5f), std::floor(camera.y * layer.speed + 0.5f), viewSize.x, viewSize.y };
}

void LayerStack::Draw(Graphics& graphics, ID3D12GraphicsCommandList* commandList, XMFLOAT2 camera, XMFLOAT2 viewSize) const
{
	for (const SceneLayer& layer : m_layers)
	{
		if (!layer.visible)
		{
			continue;
		}

		const RectF view = ViewOf(layer, camera, viewSize);

		if (layer.drawSprites)
		{
			graphics.sprites.Begin(view);
			layer.drawSprites(graphics.sprites, view);
			graphics.sprites.End(commandList);
		}

		if (layer.drawText)
		{
			graphics.text.Begin(commandList, view);
			layer.drawText(graphics.text, view);
			graphics.text.End();
		}
	}
}

Scene::Scene(const Background& background, bool frontLayer, float frontSpeedPercent) :
	m_backgroundCount(background.Layers()),
	m_action(0),
	m_front(-1),
	m_indicator(0),
	m_gui(0),
	m_map(0)
{
	// 12 at most for the background, the action layer and the front layer together.
	const int roomForBackground = c_maxStageLayers - 1 - (frontLayer ? 1 : 0);
	if (m_backgroundCount > roomForBackground)
	{
		OutputDebugStringA("Scene: more background layers than fit in 12 with the action and front layers; the nearest ones are left out.\n");
		m_backgroundCount = roomForBackground;
	}

	for (int i = 0; i < m_backgroundCount; i++)
	{
		m_layers.Push(MakeLayer("background", background.Speed(i)));
	}

	m_action = m_layers.Push(MakeLayer("action", 1.0f));

	if (frontLayer)
	{
		const float speed = (frontSpeedPercent < 1.0f ? 1.0f : frontSpeedPercent) / 100.0f;
		m_front = static_cast<int>(m_layers.Push(MakeLayer("front", speed)));
	}

	m_indicator = m_layers.Push(MakeLayer("indicator", 1.0f));
	m_gui = m_layers.Push(MakeLayer("gui", 0.0f, true));
	m_map = m_layers.Push(MakeLayer("map", 0.0f, true));
	m_layers[m_map].visible = false;
}

void Scene::Follow(XMFLOAT2 target, XMFLOAT2 levelSize)
{
	const float maxX = levelSize.x > viewSize.x ? levelSize.x - viewSize.x : 0.0f;
	const float maxY = levelSize.y > viewSize.y ? levelSize.y - viewSize.y : 0.0f;
	camera.x = Clamp(target.x - viewSize.x * 0.5f, 0.0f, maxX);
	camera.y = Clamp(target.y - viewSize.y * 0.5f, 0.0f, maxY);
}

void MechaMan::DrawRepeatingX(SpriteRenderer& sprites, const Texture* texture, const RectF& view, float y, int layer)
{
	if (texture == nullptr || texture->GetWidth() == 0)
	{
		return;
	}

	const float width = static_cast<float>(texture->GetWidth());
	for (float x = std::floor(view.x / width) * width; x < view.x + view.width; x += width)
	{
		sprites.DrawTexture(texture, XMFLOAT2{ x, y }, 1.0f, layer);
	}
}
