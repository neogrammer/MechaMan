#pragma once
#include "../Content/SpriteRenderer.h"
#include <vector>

namespace MechaMan
{
	struct Tileset
	{
		
		int pitch{};

		int tw{};
		int th{};

		std::vector<Sprite> tiles{};

		void addTile(Sprite& tile);
		void setTSet(int pitch_, int width_, int height_); 


	};
}