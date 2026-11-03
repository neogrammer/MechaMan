#include "pch.h"
#include "Background.h"

using namespace MechaMan;

namespace
{
	// The default speed of a layer, in percent: evenly spread from 1 (farthest) to 100 (nearest).
	float EvenSpread(int layer, int layers)
	{
		if (layers == 1)
		{
			return 50.0f;
		}
		return 1.0f + 99.0f * static_cast<float>(layer) / static_cast<float>(layers - 1);
	}
}

Background::Background(int layers, BackgroundMode mode, std::vector<float> speedPercents) :
	m_mode(mode)
{
	if (layers < 1)
	{
		OutputDebugStringA("Background: it needs at least 1 layer, so it has 1.\n");
		layers = 1;
	}
	if (layers > c_maxLayers)
	{
		OutputDebugStringA("Background: at most 11 layers (12 in all with the action layer), so the nearest ones are left out.\n");
		layers = c_maxLayers;
	}

	m_speeds.assign(static_cast<size_t>(layers), 0.0f);
	if (mode == BackgroundMode::Stationary)
	{
		return;
	}

	if (!speedPercents.empty() && speedPercents.size() != m_speeds.size())
	{
		OutputDebugStringA("Background: the number of speeds does not match the number of layers; missing ones are spread evenly.\n");
	}

	for (int i = 0; i < layers; i++)
	{
		float percent = static_cast<size_t>(i) < speedPercents.size() ? speedPercents[static_cast<size_t>(i)] : EvenSpread(i, layers);
		if (percent < 1.0f)
		{
			percent = 1.0f;
		}
		if (percent > 100.0f)
		{
			percent = 100.0f;
		}
		m_speeds[static_cast<size_t>(i)] = percent / 100.0f;
	}
}
