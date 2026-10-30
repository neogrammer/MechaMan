#pragma once
#include "../Content/Rect.h"
#include "../Content/PairF.h"
#include "../Content/SpriteRenderer.h"
namespace MechaMan
{
	struct Tile
	{
		inline int tw() const { return spr.texRect.width; }
		inline int th() const { return spr.texRect.height; }

		Sprite spr{};

		PairF unIso() const;

		void setSprite(const Sprite& spr_);
	};
}