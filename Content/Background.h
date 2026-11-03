#pragma once

#include <vector>

namespace MechaMan
{
	enum class BackgroundMode
	{
		Stationary,		// the background layers stay put on screen while the camera moves
		Parallax,		// each layer moves at its own fraction of the action layer's speed
	};

	// How the layers behind the action layer move: how many there are, and how fast each
	// scrolls compared with the action layer. What they show is set on the Scene.
	class Background
	{
	public:
		// 12 layers in all for the background, the action layer and the front layer, so up to
		// 11 here (10 when the scene has a front layer too).
		static constexpr int c_maxLayers = 11;

		// 'layers': how many, farthest first; at least 1.
		// 'speedPercents' (Parallax only): how fast each layer moves compared with the action
		// layer, from 1 to 100, farthest first. Leave it out to spread the layers evenly from 1%
		// (the farthest) to 100% (the nearest); a single layer moves at 50%. Missing entries are
		// filled in the same way.
		Background(int layers, BackgroundMode mode, std::vector<float> speedPercents = {});

		int Layers() const { return static_cast<int>(m_speeds.size()); }
		BackgroundMode Mode() const { return m_mode; }

		// As a fraction of the action layer's speed: 0.01 to 1, or 0 when Stationary.
		float Speed(int layer) const { return m_speeds[static_cast<size_t>(layer)]; }

	private:
		BackgroundMode		m_mode;
		std::vector<float>	m_speeds;
	};
}
