#pragma once
#include <vector>
#include "Tileset.h"
#include "../Content/SpriteRenderer.h"

namespace MechaMan
{
	class Tilemap
	{
		Tileset m_tset{};
		int m_cols{};
		int m_rows{};
		int m_id{};

		std::vector<Tile> m_tiles{};

	public:

		void setup(const std::vector<Tile>& tiles_, int cols_, int rows_, int id_, int pitch_, int* tilemapData_);
		Sprite&& sprAt(int index_);
		Sprite&& sprAt(int col_, int row_);
		inline std::vector<Tile>& getTiles() { return m_tiles; }
		Tileset& getTSet() { return m_tset; }
	};
}