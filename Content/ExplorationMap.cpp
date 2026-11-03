#include "pch.h"
#include "ExplorationMap.h"

#include "TextRenderer.h"

#include <cmath>

using namespace MechaMan;
using namespace DirectX;

namespace
{
	const float c_cellHeight = 54.0f;		// how tall a cell is drawn, screen units; the width keeps the cell's shape
	const float c_panSpeed = 500.0f;		// screen units per second

	const XMFLOAT4 c_dim = { 0.0f, 0.0f, 0.0f, 0.55f };
	const XMFLOAT4 c_cellEdge = { 0.55f, 0.75f, 1.0f, 0.9f };
	const XMFLOAT4 c_cellFill = { 0.12f, 0.3f, 0.75f, 0.7f };
	const XMFLOAT4 c_white = { 1.0f, 1.0f, 1.0f, 1.0f };
	const XMFLOAT4 c_grey = { 0.7f, 0.7f, 0.75f, 1.0f };

	int ToCell(float value, float cellSize, int cells)
	{
		const int cell = static_cast<int>(std::floor(value / cellSize));
		return cell < 0 ? 0 : (cell >= cells ? cells - 1 : cell);
	}
}

ExplorationMap::ExplorationMap(XMFLOAT2 levelSize, XMFLOAT2 cellSize) :
	m_cellSize(cellSize),
	m_columns(1),
	m_rows(1),
	m_open(false),
	m_pan{ 0.0f, 0.0f }
{
	if (m_cellSize.x > 0.0f && m_cellSize.y > 0.0f)
	{
		m_columns = static_cast<int>(std::ceil(levelSize.x / m_cellSize.x));
		m_rows = static_cast<int>(std::ceil(levelSize.y / m_cellSize.y));
	}
	m_columns = m_columns < 1 ? 1 : m_columns;
	m_rows = m_rows < 1 ? 1 : m_rows;
	m_explored.assign(static_cast<size_t>(m_columns) * static_cast<size_t>(m_rows), 0);
}

void ExplorationMap::Visit(XMFLOAT2 worldPosition)
{
	const int column = ToCell(worldPosition.x, m_cellSize.x, m_columns);
	const int row = ToCell(worldPosition.y, m_cellSize.y, m_rows);
	m_explored[Index(column, row)] = 1;
}

bool ExplorationMap::IsExplored(int column, int row) const
{
	if (column < 0 || row < 0 || column >= m_columns || row >= m_rows)
	{
		return false;
	}
	return m_explored[Index(column, row)] != 0;
}

void ExplorationMap::Open()
{
	m_open = true;
	m_pan = { 0.0f, 0.0f };
}

void ExplorationMap::Update(const InputManager& input, float dt)
{
	if (!m_open)
	{
		return;
	}

	// Pressing right shows more of what is to the right, so the map itself moves left.
	const float step = c_panSpeed * dt;
	if (input.IsDown(Action::Right))
	{
		m_pan.x -= step;
	}
	if (input.IsDown(Action::Left))
	{
		m_pan.x += step;
	}
	if (input.IsDown(Action::Down))
	{
		m_pan.y -= step;
	}
	if (input.IsDown(Action::Up))
	{
		m_pan.y += step;
	}
}

void ExplorationMap::Draw(SpriteRenderer& sprites, XMFLOAT2 playerPosition, float seconds) const
{
	sprites.DrawRectangle(c_view, c_dim, -1);

	const float cellHeight = c_cellHeight;
	const float cellWidth = m_cellSize.y > 0.0f ? c_cellHeight * m_cellSize.x / m_cellSize.y : c_cellHeight;

	// Where the player is on the drawn map; the map is placed so that point is in the middle of
	// the screen, then moved by however far it has been panned.
	const XMFLOAT2 playerOnMap = { playerPosition.x / m_cellSize.x * cellWidth, playerPosition.y / m_cellSize.y * cellHeight };
	const XMFLOAT2 origin =
	{
		c_view.x + c_view.width * 0.5f + m_pan.x - playerOnMap.x,
		c_view.y + c_view.height * 0.5f + m_pan.y - playerOnMap.y,
	};

	const float edge = 2.0f;
	for (int row = 0; row < m_rows; row++)
	{
		for (int column = 0; column < m_columns; column++)
		{
			if (!IsExplored(column, row))
			{
				continue;
			}

			const RectF cell = { origin.x + column * cellWidth, origin.y + row * cellHeight, cellWidth, cellHeight };
			sprites.DrawRectangle(cell, c_cellEdge, 0);
			sprites.DrawRectangle({ cell.x + edge, cell.y + edge, cell.width - edge * 2.0f, cell.height - edge * 2.0f }, c_cellFill, 1);
		}
	}

	// The player's mark blinks twice a second.
	const float blink = 0.5f + 0.5f * std::cos(seconds * XM_2PI * 2.0f);
	const float mark = 10.0f;
	sprites.DrawRectangle({ origin.x + playerOnMap.x - mark * 0.5f, origin.y + playerOnMap.y - mark, mark, mark }, { 1.0f, 0.9f, 0.3f, blink }, 2);
}

void ExplorationMap::DrawLabels(TextRenderer& text) const
{
	const float lineHeight = text.Measure(L"A").y;
	if (lineHeight <= 0.0f)
	{
		return;
	}
	const float x = c_view.x + c_view.width * 0.5f;
	text.Draw(L"MAP", { x, c_view.y + c_view.height * 0.1f }, c_white, c_view.height * 0.06f / lineHeight, true);
	text.Draw(L"MOVE TO LOOK AROUND     MAP OR BACK TO CLOSE", { x, c_view.y + c_view.height * 0.9f }, c_grey, c_view.height * 0.03f / lineHeight, true);
}
