#include "pch.h"
#include "InputTestState.h"

#include <string>

using namespace MechaMan;
using namespace DirectX;

namespace
{
	// The names on screen, in the same order as the Action enum.
	const wchar_t* const c_actionNames[] =
	{
		L"LEFT",
		L"RIGHT",
		L"UP",
		L"DOWN",
		L"JUMP",
		L"SHOOT",
		L"CONFIRM",
		L"BACK",
		L"START",
	};
	static_assert(_countof(c_actionNames) == c_actionCount, "Every action needs a name here, in the same order as the Action enum.");

	// How long Back has to be held to leave, in seconds.
	const float c_holdToLeave = 1.0f;

	// Where things go, as fractions of the view's width and height, so the screen fits
	// whatever size c_view is.
	const float c_headingY = 0.07f;
	const float c_statusY = 0.13f;
	const float c_columnsY = 0.2f;		// the column headings
	const float c_firstRowY = 0.26f;	// the middle of the first action's row
	const float c_rowSpacing = 0.065f;
	const float c_lightX = 0.08f;
	const float c_countX = 0.36f;
	const float c_hintY = 0.89f;
	const float c_barY = 0.93f;

	const XMFLOAT4 c_background = { 0.04f, 0.04f, 0.07f, 1.0f };
	const XMFLOAT4 c_panel = { 0.12f, 0.12f, 0.16f, 1.0f };
	const XMFLOAT4 c_lightOff = { 0.22f, 0.22f, 0.26f, 1.0f };
	const XMFLOAT4 c_lightOn = { 0.25f, 0.9f, 0.35f, 1.0f };
	const XMFLOAT4 c_white = { 1.0f, 1.0f, 1.0f, 1.0f };
	const XMFLOAT4 c_grey = { 0.55f, 0.55f, 0.6f, 1.0f };

	XMFLOAT2 At(float x, float y)
	{
		return { c_view.x + c_view.width * x, c_view.y + c_view.height * y };
	}

	float RowHeight()
	{
		return c_view.height * c_rowSpacing;
	}

	// The middle of the left edge of an action's row.
	XMFLOAT2 RowStart(size_t row)
	{
		return At(c_lightX, c_firstRowY + c_rowSpacing * static_cast<float>(row));
	}

	// The area the square moves around in.
	RectF Box()
	{
		const XMFLOAT2 corner = At(0.5f, c_columnsY);
		return { corner.x, corner.y, c_view.width * 0.42f, c_view.height * 0.62f };
	}

	float SquareSize()
	{
		return c_view.height * 0.06f;
	}

	float Clamp(float value, float low, float high)
	{
		return value < low ? low : (value > high ? high : value);
	}
}

InputTestState::InputTestState(GameStateStack& stack, Graphics& graphics, InputManager& input) :
	GameState(stack),
	m_graphics(graphics),
	m_input(input),
	m_loaded(false),
	m_presses{},
	m_square{},
	m_backHeld(0.0f),
	m_leaving(false)
{
	// The square starts in the middle of its box.
	const RectF box = Box();
	m_square = { box.x + box.width * 0.5f, box.y + box.height * 0.5f };
}

void InputTestState::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>&)
{
	m_loaded = true;
}

void InputTestState::ReleaseDeviceDependentResources()
{
	m_loaded = false;
}

void InputTestState::Update(DX::StepTimer const& timer)
{
	const float seconds = static_cast<float>(timer.GetElapsedSeconds());

	for (size_t i = 0; i < c_actionCount; i++)
	{
		if (m_input.WasPressed(static_cast<Action>(i)))
		{
			m_presses[i]++;
		}
	}

	// The square moves while a direction is held, and stays inside its box.
	const float distance = c_view.height * 0.6f * seconds;
	if (m_input.IsDown(Action::Left))
	{
		m_square.x -= distance;
	}
	if (m_input.IsDown(Action::Right))
	{
		m_square.x += distance;
	}
	if (m_input.IsDown(Action::Up))
	{
		m_square.y -= distance;
	}
	if (m_input.IsDown(Action::Down))
	{
		m_square.y += distance;
	}

	const RectF box = Box();
	const float half = SquareSize() * 0.5f;
	m_square.x = Clamp(m_square.x, box.x + half, box.x + box.width - half);
	m_square.y = Clamp(m_square.y, box.y + half, box.y + box.height - half);

	// Back has to be held for a moment, rather than just pressed, so that it can be tried
	// out here like everything else.
	m_backHeld = m_input.IsDown(Action::Back) ? m_backHeld + seconds : 0.0f;

	// Once is enough: the Pop only happens at the start of the next update.
	if (!m_leaving && m_backHeld >= c_holdToLeave)
	{
		m_leaving = true;
		m_stack.Pop();
	}
}

void InputTestState::Render(ID3D12GraphicsCommandList* commandList)
{
	SpriteRenderer& sprites = m_graphics.sprites;
	TextRenderer& text = m_graphics.text;

	const float rowHeight = RowHeight();
	const float lightSize = rowHeight * 0.55f;

	// The shapes first...
	sprites.Begin(c_view);
	sprites.DrawRectangle(c_view, c_background, -1);

	for (size_t i = 0; i < c_actionCount; i++)
	{
		const XMFLOAT2 row = RowStart(i);
		const bool held = m_input.IsDown(static_cast<Action>(i));
		sprites.DrawRectangle({ row.x, row.y - lightSize * 0.5f, lightSize, lightSize }, held ? c_lightOn : c_lightOff);
	}

	const RectF box = Box();
	const float size = SquareSize();
	sprites.DrawRectangle(box, c_panel);
	sprites.DrawRectangle({ m_square.x - size * 0.5f, m_square.y - size * 0.5f, size, size }, c_white, 1);

	// A bar that fills up while Back is held.
	const XMFLOAT2 bar = At(0.5f, c_barY);
	const float barWidth = c_view.width * 0.25f;
	const float barHeight = c_view.height * 0.012f;
	const float filled = Clamp(m_backHeld / c_holdToLeave, 0.0f, 1.0f);
	sprites.DrawRectangle({ bar.x - barWidth * 0.5f, bar.y, barWidth, barHeight }, c_lightOff);
	sprites.DrawRectangle({ bar.x - barWidth * 0.5f, bar.y, barWidth * filled, barHeight }, c_white, 1);

	sprites.End(commandList);

	// ...then the words on top of them. Without a font, only the shapes are drawn.
	const float lineHeight = text.Measure(L"A").y;
	if (lineHeight <= 0.0f)
	{
		return;
	}
	const float scale = rowHeight * 0.6f / lineHeight;
	const float headingScale = rowHeight * 0.9f / lineHeight;
	const float halfLine = lineHeight * scale * 0.5f;	// from the middle of a row up to the top of its text
	const float nameX = c_view.x + c_view.width * c_lightX + lightSize * 1.6f;
	const float countX = c_view.x + c_view.width * c_countX;

	text.Begin(commandList);

	text.Draw(L"INPUT TEST", At(0.5f, c_headingY), c_white, headingScale, true);
	if (m_input.IsControllerConnected())
	{
		text.Draw(L"CONTROLLER CONNECTED", At(0.5f, c_statusY), c_lightOn, scale, true);
	}
	else
	{
		text.Draw(L"NO CONTROLLER", At(0.5f, c_statusY), c_grey, scale, true);
	}

	const float columnsY = c_view.y + c_view.height * c_columnsY;
	text.Draw(L"ACTION", { nameX, columnsY - halfLine }, c_grey, scale);
	text.Draw(L"PRESSES", { countX, columnsY - halfLine }, c_grey, scale);

	for (size_t i = 0; i < c_actionCount; i++)
	{
		const XMFLOAT2 row = RowStart(i);
		const bool held = m_input.IsDown(static_cast<Action>(i));
		text.Draw(c_actionNames[i], { nameX, row.y - halfLine }, held ? c_white : c_grey, scale);
		text.Draw(std::to_wstring(m_presses[i]), { countX, row.y - halfLine }, held ? c_white : c_grey, scale);
	}

	text.Draw(L"HOLD BACK TO RETURN", At(0.5f, c_hintY), c_grey, scale, true);

	text.End();
}
