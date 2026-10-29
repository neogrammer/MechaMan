#include "pch.h"
#include "GameplayState.h"

#include "InputTestState.h"
#include "PackageFiles.h"

#include <cmath>

using namespace MechaMan;

GameplayState::GameplayState(GameStateStack& stack, Graphics& graphics, InputManager& input) :
	GameState(stack),
	m_graphics(graphics),
	m_input(input),
	m_loaded(false),
	m_tiles{},
	m_tileset(nullptr)
{}

void GameplayState::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>&)
{
	// The splash screen loaded this already: GetTexture hands back the same texture.
	if (PackageFiles::Exists(L"Assets\\Tileset1.png"))
	{
		m_tileset = m_graphics.sprites.GetTexture(L"Assets\\Tileset1.png");

		// Tileset1.png is a 25 x 25 grid of cells 169 pixels square (4225 / 25), so that is
		// the step from one tile's texRect to the next.
		const int cellSize = 169;

		// Each tile is drawn 128 view units square. 1280 / 128 = 10 fill the view across, and
		// 720 / 128 = 5.6, so 6 rows fill it down (the last one runs off the bottom).
		const float tileSize = 128.0f;
		const int columns = static_cast<int>(std::ceil(c_view.width / tileSize));
		const int rows = static_cast<int>(std::ceil(c_view.height / tileSize));

		struct p
		{
			float x;
			float y;
		};

		auto lammy = [tileSize](int x, int y, int tileW, int tileH)->p { 
			float tempx = (x - y) * tileW / 2.f;
			float tempy = (x + y) * tileH / 4.f;
			p newp = p{ tempx, tempy };
			return newp;
			};

		for (int y = 0; y < rows; ++y)
		{
			for (int x = 0; x < columns; ++x)
			{
				Sprite tile;
				tile.texture = m_tileset.get();
				tile.SetTexRect({ x * cellSize, y * cellSize, cellSize, cellSize });
				tile.position = { c_view.x + x * tileSize, c_view.y + y * tileSize };
				p iso = p{ lammy(x, y, 128, 128) };
				tile.position = { iso.x, iso.y };
				tile.size = { tileSize, tileSize };
				m_tiles.push_back(tile);
				
				
			}
		}
	}
	m_loaded = true;
}

void GameplayState::ReleaseDeviceDependentResources()
{
	m_tileset = nullptr;
	m_tiles.clear();
	m_loaded = false;
}

void GameplayState::Update(DX::StepTimer const& timer)
{

	// PRESS START: Enter, Space, Escape, or the controller's A or Menu button.
	if (m_input.WasPressed(Action::Start) || m_input.WasPressed(Action::Confirm))
	{
		// TODO: when there is a first level, Replace the title with it here instead. For now
		// the input test screen goes on top, and pops itself to come back here.
		m_stack.Push(std::make_unique<InputTestState>(m_stack, m_graphics, m_input));
	}
}

void GameplayState::Render(ID3D12GraphicsCommandList* commandList)
{
	// Everything fades in out of black over half a second.
	m_graphics.sprites.Begin(c_view);
	m_graphics.sprites.DrawRectangle(c_view, { 0.0f, 0.0f, 0.0f, 1.0f }, -1);
	if (m_tileset != nullptr)
	{
		//const RectF top = { c_view.x, c_view.y, c_view.width, c_view.height };
		if (!m_tiles.empty())
		{
			for (auto& tile : m_tiles)
			{
				m_graphics.sprites.DrawTexture(tile.texture, tile.texRect, RectF{ tile.position.x, tile.position.y, tile.size.x, tile.size.y }, 1.f); //FitInside(1400, 900, top, 1.f), 1.f);
			}
		}
	}

	// End has to follow every Begin, even without a tileset, or the next frame's Begin
	// finds the batch still open.
	m_graphics.sprites.End(commandList);

	// PRESS START blinks once a second.
	m_graphics.text.Begin(commandList);
	m_graphics.text.Draw(L"PRESS START", { c_view.x + c_view.width * 0.5f, c_view.y + c_view.height * 0.8f }, { 1.0f, 1.0f, 1.0f, 1.f }, 1.0f, true);
	m_graphics.text.End();
}
