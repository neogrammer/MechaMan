#pragma once

#include "SpriteRenderer.h"	// Rect

#include <optional>
#include <string>
#include <vector>

namespace MechaMan
{
	// Where the hand is on one frame, and which way a held weapon points, so a sword that is not
	// part of the artwork can be drawn in it. Anchor-relative like the boxes, sheet pixels, at the
	// middle of the hand's pixel.
	struct HandPose
	{
		DirectX::XMFLOAT2 at;
		float angle = 0.0f;		// radians: 0 points forward, negative turns it up (clockwise is positive)
		bool strike = false;	// the blade can hurt on this frame
	};

	// One animation from a sheet made by the Sprite Sheet Processor: frame i is a texRect on
	// the sheet, ready for Sprite::SetTexRect.
	//
	// The boxes are anchor-relative, in the sheet's pixels. The anchor is the bottom centre of
	// the cell, so a box's y is negative above the feet; a Sprite with origin (0.5, 1) is placed
	// by the same point. Facing is handled by mirroring around the anchor (Sprite::flipX), so
	// no flipped frames are stored.
	struct AnimationData
	{
		std::string name;
		std::vector<Rect> frames;					// texRects, one per frame
		float secondsPerFrame = 0.1f;
		bool loop = true;
		std::string next;							// empty: stop on the last frame

		Rect hurt = { 0, 0, 0, 0 };					// the body; all zeros means the whole cell
		std::vector<std::optional<Rect>> hits;		// one slot per frame; empty slot = no hit box
		std::vector<std::optional<HandPose>> hands;	// one slot per frame; empty slot = nothing in hand
	};

	// Cell coordinates (0, 0 = top left of the cell) to anchor-relative.
	inline Rect ToAnchorRelative(Rect box, int cellWidth, int cellHeight)
	{
		box.x -= cellWidth / 2;
		box.y -= cellHeight;
		return box;
	}
}
