#pragma once

#include "InputManager.h"
#include "SpriteRenderer.h"

#include <cstdint>
#include <vector>

namespace MechaMan
{
	class TextRenderer;

	// The map of where the player has been, Metroid and Castlevania style. The level is split
	// into cells (one screen each, say). A cell is revealed the first time the player stands in
	// it, and only revealed cells are ever drawn. While the map is open it lies over the whole
	// screen, see-through, and the directions move it around; the owning state stops updating
	// the world meanwhile, so the player stays where they are.
	class ExplorationMap
	{
	public:
		ExplorationMap(DirectX::XMFLOAT2 levelSize, DirectX::XMFLOAT2 cellSize);

		// Reveals the cell under a point of the level (world units).
		void Visit(DirectX::XMFLOAT2 worldPosition);
		bool IsExplored(int column, int row) const;

		bool IsOpen() const { return m_open; }
		void Open();					// centred on the player again
		void Close() { m_open = false; }

		// While open: the directions move the map around.
		void Update(const InputManager& input, float dt);

		// In screen units (c_view), inside the map layer's batches: the screen dimmed, the
		// revealed cells, and a blinking mark where the player is.
		void Draw(SpriteRenderer& sprites, DirectX::XMFLOAT2 playerPosition, float seconds) const;
		void DrawLabels(TextRenderer& text) const;

	private:
		size_t Index(int column, int row) const { return static_cast<size_t>(row) * static_cast<size_t>(m_columns) + static_cast<size_t>(column); }

		DirectX::XMFLOAT2		m_cellSize;
		int						m_columns;
		int						m_rows;
		std::vector<uint8_t>	m_explored;
		bool					m_open;
		DirectX::XMFLOAT2		m_pan;			// screen units, away from centred on the player
	};
}
