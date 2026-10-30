#pragma once
#include "../Content/SpriteRenderer.h"
#include <vector>
#include "Tile.h"
namespace MechaMan
{
	struct Tileset
	{
		
		int pitch{};

		std::vector<Tile> tiles{};

		void addTile(const Tile& tile_);
		void setTSet(int pitch_); 
		
		inline int tw(int index) { return tiles[0].tw(); }
		inline int th(int index) { return tiles[0].th(); }

	};
}