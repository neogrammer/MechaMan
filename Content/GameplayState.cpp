#include "pch.h"
#include "GameplayState.h"

#include "InputTestState.h"
#include "PackageFiles.h"

#include <cmath>
#include "PairF.h"

using namespace MechaMan;

GameplayState::GameplayState(GameStateStack& stack, Graphics& graphics, InputManager& input) :
	GameState(stack),
	m_graphics(graphics),
	m_input(input),
	m_loaded(false),
	m_tileset(nullptr),
	m_tmap{}
{}

void GameplayState::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>&)
{




	// The splash screen loaded this already: GetTexture hands back the same texture.
	if (PackageFiles::Exists(L"Assets\\tilesets\\grass_tiles.png"))
	{
		m_tileset = m_graphics.sprites.GetTexture(L"Assets\\tilesets\\grass_tiles.png");

		// grass_tiles.png is a 8 x 7 grid of cells 128 pixels square (1024 / 8), so that is
		// the step from one tile's texRect to the next.
		const int cellSize = 128;

		// Each tile is drawn 128 view units square.  7 rows fill it
		const float tileSize = 128.0f;
	   
		const int columns = static_cast<int>(std::ceil(m_tileset->GetWidth() / tileSize));
		const int rows = static_cast<int>(std::ceil(m_tileset->GetHeight() / tileSize));

		auto lammy = [tileSize](int x, int y, int tileW, int tileH)->PairF {
			float tempx = (x - y) * tileW / 2.f;
			float tempy = (x + y) * tileH / 4.f;
			PairF newp = PairF{ tempx, tempy };
			return newp;
			};


		Tileset t{};
		t.tiles.reserve(7 * 8);
		for (int y = 0; y < 7; y++)
		{
			for (int x = 0; x < 8; x++)
			{
				t.tiles.push_back(Tile{});
				Sprite temp{};
				temp.texture = m_tileset.get();
				temp.SetTexRect({  x * (int)tileSize , y * (int)tileSize, (int)tileSize, (int)tileSize });
				temp.color = { 1.0f, 1.0f, 1.0f, 1.0f };
				temp.flipX = false;
				temp.flipY = false;
				temp.layer = 0;
				temp.origin = { 0.0f, 0.0f };
				temp.position = { (float)x * tileSize, (float)y * tileSize };
				temp.rotation = 0.0f;
				temp.size = { tileSize, tileSize };
			
				t.tiles.back().setSprite(temp);
			}

		}
		int* map = new int[20 * 20]{
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

		m_tmap.setup(t.tiles, 20, 20, 0, 8, map);
	




		for (int y = 0; y < rows; ++y)
		{
			for (int x = 0; x < columns; ++x)
			{
				Sprite tile;
				tile.texture = m_tileset.get();
				tile.SetTexRect({ x * cellSize, y * cellSize, cellSize, cellSize });
				tile.position = { c_view.x + x * tileSize, c_view.y + y * tileSize };
				PairF iso = PairF{ lammy(x, y, 128, 128) };
				tile.position = { iso.x, iso.y };
				tile.position.x += c_viewOffset.x * tileSize;
				tile.position.y += c_viewOffset.y * tileSize;
				tile.size = { tileSize, tileSize };
				Tile temp{};
				temp.setSprite(tile);
				m_tmap.getTSet().addTile(temp);
			}
		}
	}
	m_loaded = true;
}

void GameplayState::ReleaseDeviceDependentResources()
{
	m_tmap.getTSet().tiles.clear();
	m_tileset = nullptr;
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
	if (m_tileset != nullptr)	{	if (!m_tmap.getTSet().tiles.empty())	{
			for (auto& tile : m_tmap.getTiles())
				m_graphics.sprites.DrawTexture(tile.spr.texture, tile.spr.texRect, RectF{ tile.spr.position.x + (c_viewOffset.x * tile.tw()), tile.spr.position.y + (c_viewOffset.y * tile.th()), tile.spr.size.x, tile.spr.size.y}, 1.f); //FitInside(1400, 900, top, 1.f), 1.f);
	}	}

	// End has to follow every Begin, even without a tileset, or the next frame's Begin
	// finds the batch still open.
	m_graphics.sprites.End(commandList);

	// PRESS START blinks once a second.
	//m_graphics.text.Begin(commandList);
	//m_graphics.text.Draw(L"PRESS START", { c_view.x + c_view.width * 0.5f, c_view.y + c_view.height * 0.8f }, { 1.0f, 1.0f, 1.0f, 1.f }, 1.0f, true);
	//m_graphics.text.End();
}
