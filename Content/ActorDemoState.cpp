#include "pch.h"
#include "ActorDemoState.h"

#include "PackageFiles.h"

#include <GraphicsMemory.h>

#include <cstdio>
#include <set>
#include <string>

using namespace MechaMan;
using namespace DirectX;

namespace
{
	const wchar_t* c_sheetFile = L"Assets\\alucard.cfg";
	const wchar_t* c_stageFile = L"Assets\\cfg\\intro_stage.cfg";
	const wchar_t* c_enemyFolder = L"Assets\\cfg\\";		// an enemy's kind is the name of its .cfg here

	// intro.png holds two layers of the stage in one file: the highway along the top, with
	// clear sky around it, and the city at dusk in a strip lower down (224 high, the height of
	// the original screen).
	const wchar_t* c_stageArtFile = L"Assets\\backgrounds\\intro.png";
	const Rect c_highway = { 0, 0, 7689, 400 };		// the whole highway, one world unit per pixel
	const Rect c_city = { 9, 490, 3545, 224 };		// the city strip
	const float c_cityTop = -20.0f;					// the strip's top edge on screen: a little of its sky is above it
	const float c_citySpeed = 30.0f;				// percent of the action layer's speed, across

	// The camera shows 320 x 180 world units: 16:9, and a whole number of times bigger on any
	// screen. It aims a little above the player's feet, so they stand in the lower half.
	const XMFLOAT2 c_viewSize = { 320.0f, 180.0f };
	const float c_lookAbove = 30.0f;

	// After dying, how long before the stage starts again from the last checkpoint.
	const float c_restartDelay = 2.0f;

	// The longest step the world takes in one update, in seconds, so a pause in the debugger
	// or a dragged window does not send everyone flying.
	const float c_maxStep = 0.1f;

	const XMFLOAT4 c_white = { 1.0f, 1.0f, 1.0f, 1.0f };
	const XMFLOAT4 c_grey = { 0.65f, 0.65f, 0.7f, 1.0f };

	XMFLOAT4 Color(int red, int green, int blue, float alpha = 1.0f)
	{
		return { red / 255.0f, green / 255.0f, blue / 255.0f, alpha };
	}

	RectF ToRectF(const Rect& rect)
	{
		return { static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width), static_cast<float>(rect.height) };
	}

	// The stage, or an empty one (no checkpoints) when it did not load.
	Stage LoadStage(const wchar_t* file)
	{
		Stage stage;
		return stage.Load(file) ? stage : Stage();
	}

	// A health bar along the top of the screen, screen units.
	void DrawHealthBar(SpriteRenderer& sprites, float x, float y, float fraction, XMFLOAT4 color)
	{
		const float width = 280.0f;
		const float height = 16.0f;
		fraction = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);

		sprites.DrawRectangle({ x - 3.0f, y - 3.0f, width + 6.0f, height + 6.0f }, Color(0, 0, 0, 0.7f), 0);
		sprites.DrawRectangle({ x, y, width * fraction, height }, color, 1);
	}

	// A line of text centred at y, shrunk if it would not fit across the screen.
	void DrawCentredLine(TextRenderer& text, const wchar_t* line, float y, float scale, XMFLOAT4 color)
	{
		const float widest = c_view.width * 0.96f;
		const float width = text.Measure(line).x * scale;
		if (width > widest)
		{
			scale *= widest / width;
		}
		text.Draw(line, { c_view.x + c_view.width * 0.5f, y }, color, scale, true);
	}
}

ActorDemoState::ActorDemoState(GameStateStack& stack, Graphics& graphics, InputManager& input) :
	GameState(stack),
	m_graphics(graphics),
	m_input(input),
	m_stage(LoadStage(c_stageFile)),
	// One background layer, the city, at 30%; no front layer yet.
	m_scene(Background(1, BackgroundMode::Parallax, { c_citySpeed }), false),
	m_map(m_stage.size, c_viewSize),
	m_checkpoint(0),
	m_deadFor(0.0f),
	m_playerMaxHealth(1),
	m_seconds(0.0f),
	m_loaded(false),
	m_debugBoxes(false)
{
	m_scene.viewSize = c_viewSize;

	// The animations are plain data, so they load now; the textures wait for the device.
	// Alucard, then one sheet for every kind of enemy in the stage.
	const SpriteSheet* alucard = m_sheets.Load(c_sheetFile);

	std::set<std::string> kinds;
	for (const EnemySpawn& spawn : m_stage.enemies)
	{
		if (kinds.insert(spawn.kind).second)
		{
			m_sheets.Load(c_enemyFolder + std::wstring(spawn.kind.begin(), spawn.kind.end()) + L".cfg");
		}
	}

	if (alucard && !m_stage.checkpoints.empty())
	{
		StartWorld();
		SetUpLayers();
	}
}

void ActorDemoState::StartWorld()
{
	m_world = std::make_unique<World>(m_sheets, m_stage, m_checkpoint);
	m_playerMaxHealth = m_world->GetPlayer().health;
	m_deadFor = 0.0f;

	const XMFLOAT2 player = m_world->GetPlayer().position;
	m_scene.Follow({ player.x, player.y - c_lookAbove }, m_stage.size);
	m_map.Visit(player);
}

void ActorDemoState::SetUpLayers()
{
	// The background: the city at dusk, sliding by at 30% as the camera moves across. It
	// stays put up and down (the strip is only one screen high). Behind it, the colour of the
	// strip's sky, in case anything shows past it.
	m_scene.BackgroundLayer(0).drawSprites = [this](SpriteRenderer& sprites, const RectF& view)
	{
		sprites.DrawRectangle(view, Color(49, 48, 66), -1);
		const RectF city = { 0.0f, view.y + c_cityTop, static_cast<float>(c_city.width), static_cast<float>(c_city.height) };
		sprites.DrawTexture(m_stageArt.get(), c_city, city);
	};

	// The action layer: the highway, then the World on top of it. The art is drawn at one
	// world unit per pixel, so the roads in the picture and the stage's solids line up.
	m_scene.ActionLayer().drawSprites = [this](SpriteRenderer& sprites, const RectF&)
	{
		sprites.DrawTexture(m_stageArt.get(), c_highway, ToRectF(c_highway), 1.0f, -1);
		m_world->Draw(sprites, m_debugBoxes);
	};

	// Indicators: damage numbers over whoever was hit.
	m_scene.IndicatorLayer().drawText = [this](TextRenderer& text, const RectF&)
	{
		m_numbers.Draw(text);
	};

	// The GUI: the health bar, and the controls along the bottom.
	m_scene.GuiLayer().drawSprites = [this](SpriteRenderer& sprites, const RectF&)
	{
		DrawHealthBar(sprites, 40.0f, 44.0f, static_cast<float>(m_world->GetPlayer().health) / m_playerMaxHealth, Color(70, 200, 90));
	};
	m_scene.GuiLayer().drawText = [this](TextRenderer& text, const RectF&)
	{
		const float lineHeight = text.Measure(L"A").y;
		if (lineHeight <= 0.0f)
		{
			return;
		}
		const float scale = c_view.height * 0.03f / lineHeight;

		text.Draw(L"ALUCARD", { 40.0f, 14.0f }, c_white, scale);

		// With the boxes on: how much memory DirectXTK holds for drawing text. It should settle
		// within a second or two and stay put; if it keeps climbing, frames are not being
		// handed back (GraphicsMemory::Commit in DeviceResources::Present).
		if (m_debugBoxes)
		{
			const GraphicsMemoryStatistics stats = GraphicsMemory::Get().GetStatistics();
			wchar_t line[96];
			swprintf_s(line, L"TEXT MEMORY %.2f MB IN %u PAGES", stats.totalMemory / (1024.0 * 1024.0), static_cast<unsigned>(stats.totalPages));
			text.Draw(line, { c_view.width - 560.0f, 14.0f }, c_grey, scale);
		}
		DrawCentredLine(text, L"KEYBOARD   WALK A D   JUMP SPACE   ATTACK J   DASH SHIFT   CROUCH S   MAP M   BOXES ENTER   BACK ESC",
			c_view.height * 0.91f, scale, c_grey);
		DrawCentredLine(text, L"CONTROLLER   WALK STICK   JUMP A   ATTACK X   DASH RB   CROUCH DOWN   MAP VIEW   BOXES MENU   BACK B",
			c_view.height * 0.95f, scale, c_grey);
	};

	// The map, over everything, while it is open.
	m_scene.MapLayer().drawSprites = [this](SpriteRenderer& sprites, const RectF&)
	{
		m_map.Draw(sprites, m_world->GetPlayer().position, m_seconds);
	};
	m_scene.MapLayer().drawText = [this](TextRenderer& text, const RectF&)
	{
		m_map.DrawLabels(text);
	};
}

void ActorDemoState::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>&)
{
	m_sheets.CreateDeviceDependentResources(m_graphics.sprites);

	// A missing stage picture should not stop the game: the layers just draw nothing for it.
	if (PackageFiles::Exists(c_stageArtFile))
	{
		m_stageArt = m_graphics.sprites.GetTexture(c_stageArtFile);
	}
	else
	{
		OutputDebugStringW(L"ActorDemoState: Assets\\backgrounds\\intro.png is not in the package. Is it in the project as an Image?\n");
	}

	m_loaded = true;
}

void ActorDemoState::ReleaseDeviceDependentResources()
{
	m_sheets.ReleaseDeviceDependentResources();
	m_stageArt = nullptr;
	m_loaded = false;
}

void ActorDemoState::Update(DX::StepTimer const& timer)
{
	float dt = static_cast<float>(timer.GetElapsedSeconds());
	if (dt > c_maxStep)
	{
		dt = c_maxStep;
	}
	m_seconds += dt;

	if (m_map.IsOpen())
	{
		// The world waits while the map is open, so the player stays where they are.
		if (m_input.WasPressed(Action::Back) || m_input.WasPressed(Action::Map))
		{
			m_map.Close();
		}
		else
		{
			m_map.Update(m_input, dt);
		}
		m_scene.MapLayer().visible = m_map.IsOpen();
		return;
	}

	// Back returns to the title screen. Escape is a Start key too, so this comes first.
	if (m_input.WasPressed(Action::Back))
	{
		m_stack.Pop();
		return;
	}

	if (!m_world)
	{
		return;
	}

	if (m_input.WasPressed(Action::Map))
	{
		m_map.Open();
		m_scene.MapLayer().visible = true;
		return;
	}

	if (m_input.WasPressed(Action::Start))
	{
		m_debugBoxes = !m_debugBoxes;
	}

	m_world->ReadInput(m_input);
	m_world->SetView({ m_scene.camera.x, m_scene.camera.y, m_scene.viewSize.x, m_scene.viewSize.y });
	m_world->Update(dt);

	for (const HitReport& hit : m_world->Hits())
	{
		m_numbers.Spawn(hit.at, hit.damage);
	}
	m_numbers.Update(dt);

	const Actor& player = m_world->GetPlayer();

	// The furthest checkpoint passed is where the next try starts.
	while (player.Alive() && m_checkpoint + 1 < m_stage.checkpoints.size() &&
		player.position.x >= m_stage.checkpoints[m_checkpoint + 1].x)
	{
		++m_checkpoint;
	}

	// Beaten, or fallen into a pit: a moment to take it in, then the stage again.
	if (!player.Alive())
	{
		m_deadFor += dt;
		if (m_deadFor >= c_restartDelay)
		{
			StartWorld();
			return;
		}
	}

	if (player.position.y <= m_stage.size.y)
	{
		m_map.Visit(player.position);
	}
	m_scene.Follow({ player.position.x, player.position.y - c_lookAbove }, m_stage.size);
}

void ActorDemoState::Render(ID3D12GraphicsCommandList* commandList)
{
	if (m_world)
	{
		m_scene.Draw(m_graphics, commandList);
		return;
	}

	m_graphics.sprites.Begin(c_view);
	m_graphics.sprites.DrawRectangle(c_view, { 0.0f, 0.0f, 0.0f, 1.0f }, -1);
	m_graphics.sprites.End(commandList);

	TextRenderer& text = m_graphics.text;
	const float lineHeight = text.Measure(L"A").y;
	if (lineHeight > 0.0f)
	{
		text.Begin(commandList);
		text.Draw(L"THE INTRO STAGE OR ALUCARD DID NOT LOAD. THE OUTPUT WINDOW SAYS WHY.",
			{ c_view.x + c_view.width * 0.5f, c_view.y + c_view.height * 0.5f }, c_white, c_view.height * 0.04f / lineHeight, true);
		text.End();
	}
}
