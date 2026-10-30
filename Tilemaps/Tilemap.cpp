#include "pch.h"
#include "Tilemap.h"


void MechaMan::Tilemap::setup(const std::vector<Tile>& tiles_, int cols_, int rows_, int id_, int pitch_, int* tilemapData_)
{

	auto lammy = [&](int x, int y, int tileW, int tileH)->PairF {
		float tempx = (x - y) * tileW / 2.f;
		float tempy = (x + y) * tileH / 4.f;
		PairF newp = PairF{ tempx, tempy };
		return newp;
		};

	m_tset.tiles.reserve(tiles_.size());

	m_tset.setTSet(pitch_);

	for (auto& tile : tiles_)
	{
		m_tset.addTile(tile);
	}
	m_id = id_;
	m_cols = cols_;
	m_rows = rows_;

	m_tiles.reserve(cols_ * rows_);
	for (int row = 0; row < rows_; ++row)
	{
		for (int col = 0; col < cols_; ++col)
		{
			int index = row * cols_ + col;
			int tileIndex = tilemapData_[index];
			if (tileIndex >= 0 && tileIndex < static_cast<int>(m_tset.tiles.size()))
			{
				m_tiles.emplace_back(Tile{});
				auto& tile = m_tiles[m_tiles.size() - 1];
				tile.setSprite(m_tset.tiles[tileIndex].spr);
				auto pf = lammy(col , row, m_tset.tiles[tileIndex].spr.size.x, m_tset.tiles[tileIndex].spr.size.y);
				tile.spr.position.x = pf.x;
				tile.spr.position.y = pf.y;
			}
			else
			{
				// Handle invalid tile index, e.g., push a default tile or skip
				m_tiles.push_back(Tile{}); // Push a default tile
			}
		}
	}
}

MechaMan::Sprite&& MechaMan::Tilemap::sprAt(int index_)
{
	Sprite spr = m_tset.tiles[index_].spr;
	return static_cast<Sprite&&>(spr);

}

MechaMan::Sprite&& MechaMan::Tilemap::sprAt(int col_, int row_)
{
	int index_ = col_ * row_ + col_;
	if (index_ < 0 || index_ >= static_cast<int>(m_tset.tiles.size()))
	{
		// Handle out-of-bounds access, e.g., return a default sprite or throw an exception
		return Sprite{}; // Return a default sprite
	}
	Sprite spr = m_tset.tiles[index_].spr;
	return static_cast<Sprite&&>(spr);

}
