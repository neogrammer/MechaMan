#pragma once

#include "SpriteRenderer.h"
#include "TextRenderer.h"

namespace MechaMan
{
	// Everything a state draws with. MechaManMain owns the one Graphics and hands it to each
	// state's constructor; the state keeps a reference (Graphics& m_graphics) and uses
	// m_graphics.sprites for images and m_graphics.text for words.
	struct Graphics
	{
		SpriteRenderer	sprites;
		TextRenderer	text;
	};
}
