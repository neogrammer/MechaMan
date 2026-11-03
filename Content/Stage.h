#pragma once

// A level's ground, checkpoints and enemies, read from a stage .cfg. World units are pixels of
// the level's art. File format, one item per line, # starts a comment:
//
//   size <width> <height>                  the whole level; falling below it is falling out
//   checkpoint <x> <y>                     where the player starts again after dying, once they
//                                          have reached it; the first one is the start
//   solid <x> <y> <width> <height>         ground and walls: stood on from above, bumped into
//                                          from the sides and from below
//   ramp <x0> <y0> <x1> <y1>               a slope, walked along from x0 to x1 (left to right);
//                                          stood on from above only
//   enemy <kind> <x> <y>                   kind is the name of its sheet in Assets\cfg (rider is
//                                          Assets\cfg\rider.cfg); x, y is where its feet are
//
// PC and Xbox alike: read from the app package, so the file has to be in the project with
// Content set to Yes. Problems are written to the debugger's Output window.

#include "Rect.h"

#include <string>
#include <vector>

namespace MechaMan
{
	struct Ramp
	{
		float x0 = 0.0f, y0 = 0.0f;
		float x1 = 0.0f, y1 = 0.0f;

		// The height of the slope at x, which has to be between x0 and x1.
		float SurfaceAt(float x) const
		{
			return x1 > x0 ? y0 + (x - x0) * (y1 - y0) / (x1 - x0) : y0;
		}
	};

	struct EnemySpawn
	{
		std::string kind;
		DirectX::XMFLOAT2 at = { 0.0f, 0.0f };
	};

	struct Stage
	{
		DirectX::XMFLOAT2 size = { 320.0f, 180.0f };
		std::vector<DirectX::XMFLOAT2> checkpoints;
		std::vector<RectF> solids;
		std::vector<Ramp> ramps;
		std::vector<EnemySpawn> enemies;

		// False when the file is missing or has a line it does not understand, or has no
		// checkpoint (there would be nowhere to start).
		bool Load(const std::wstring& file);
	};
}
