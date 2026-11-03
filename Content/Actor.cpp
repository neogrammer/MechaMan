#include "pch.h"
#include "Actor.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

using namespace MechaMan;
using namespace DirectX;

namespace
{
	// Names in the .cfg are plain ASCII, so this is enough for debug messages.
	std::wstring Wide(const std::string& text)
	{
		return std::wstring(text.begin(), text.end());
	}

	// The sword: a rapier, in world units, so it keeps its length whatever size its wielder is
	// drawn at. A long thin blade narrowing to a point, a small cross guard and the grip.
	const float c_bladeLength = 26.0f;
	const float c_bladeWidth = 1.0f;
	const float c_tipLength = 7.0f;			// the last part of the blade, narrower
	const float c_tipWidth = 0.5f;
	const float c_gripLength = 3.5f;
	const float c_gripWidth = 1.5f;
	const float c_guardLength = 5.0f;		// across the blade
	const float c_guardWidth = 1.0f;
	const float c_bladeReach = 4.0f;		// how thick the blade counts as when it hits: a little more than it looks

	const XMFLOAT4 c_steel = { 0.82f, 0.86f, 0.92f, 1.0f };
	const XMFLOAT4 c_gold = { 0.86f, 0.66f, 0.22f, 1.0f };
	const XMFLOAT4 c_grip = { 0.36f, 0.2f, 0.1f, 1.0f };
}

XMFLOAT2 Actor::DrawPosition() const
{
	if (smooth)
	{
		return position;
	}
	return { std::floor(position.x + 0.5f), std::floor(position.y + 0.5f) };
}

bool Actor::CurrentHand(XMFLOAT2& hand, float& angle, bool& strike) const
{
	const auto& hands = m_animator.Data().hands;
	const size_t i = m_animator.Index();

	if (i >= hands.size() || !hands[i])
	{
		return false;
	}

	// Facing the other way mirrors the pose around the anchor: the hand's x changes side, and
	// the sword's direction is reflected left to right.
	const HandPose& pose = *hands[i];
	const XMFLOAT2 at = DrawPosition();
	const bool flipped = Flipped();

	hand = { at.x + (flipped ? -pose.at.x : pose.at.x) * scale, at.y + pose.at.y * scale };
	angle = flipped ? XM_PI - pose.angle : pose.angle;
	strike = pose.strike;
	return true;
}

Actor::Actor(const SpriteSheet& sheet, std::string name_) :
	name(std::move(name_)),
	m_sheet(&sheet)
{
	// Something is always playing, so Animator::Data() is always valid.
	Play(sheet.Has("idle") ? "idle" : sheet.First().name);
}

void Actor::Play(const std::string& animation)
{
	const AnimationData* data = m_sheet->Find(animation);
	Play(animation, data ? data->loop : false);
}

void Actor::Play(const std::string& animation, bool loop)
{
	const AnimationData* data = m_sheet->Find(animation);

	if (!data)
	{
		static std::set<std::pair<const SpriteSheet*, std::string>> warned;

		if (warned.insert({ m_sheet, animation }).second)
		{
			const std::wstring message = L"Actor: " + m_sheet->imageFile + L" has no animation '" + Wide(animation) +
				L"'; add it to the .cfg or rename one. Playing '" + Wide(m_sheet->First().name) + L"' instead.\n";
			OutputDebugStringW(message.c_str());
		}

		data = &m_sheet->First();
	}

	m_animationName = animation;
	m_animator.Play(*data, loop);
	m_struck.clear();	// a new animation is a new swing
}

// Boxes are stored relative to the anchor (bottom centre of the cell), so a facing flip is a
// reflection in x and no flipped copies exist anywhere.
RectF Actor::ToWorld(const Rect& box) const
{
	const float left = Flipped()
		? -static_cast<float>(box.x + box.width)
		: static_cast<float>(box.x);

	return { position.x + left * scale, position.y + static_cast<float>(box.y) * scale,
		static_cast<float>(box.width) * scale, static_cast<float>(box.height) * scale };
}

RectF Actor::HurtBox() const
{
	return ToWorld(m_animator.Data().hurt);
}

std::optional<RectF> Actor::HitBox() const
{
	// With a sword in hand, the danger is wherever the blade is: the box around it, from the
	// guard to the tip.
	XMFLOAT2 hand;
	float angle = 0.0f;
	bool strike = false;
	if (hasSword && CurrentHand(hand, angle, strike))
	{
		if (!strike)
		{
			return std::nullopt;
		}

		const XMFLOAT2 tip = { hand.x + std::cos(angle) * c_bladeLength, hand.y + std::sin(angle) * c_bladeLength };
		const float half = c_bladeReach * 0.5f;
		const float left = (hand.x < tip.x ? hand.x : tip.x) - half;
		const float right = (hand.x > tip.x ? hand.x : tip.x) + half;
		const float top = (hand.y < tip.y ? hand.y : tip.y) - half;
		const float bottom = (hand.y > tip.y ? hand.y : tip.y) + half;
		return RectF{ left, top, right - left, bottom - top };
	}

	const auto& hits = m_animator.Data().hits;
	const size_t i = m_animator.Index();

	if (i >= hits.size() || !hits[i])
	{
		return std::nullopt;
	}

	return ToWorld(*hits[i]);
}

bool Actor::Strike(const Actor& victim)
{
	if (std::find(m_struck.begin(), m_struck.end(), &victim) != m_struck.end())
	{
		return false;
	}

	m_struck.push_back(&victim);
	return true;
}

void Actor::Update(float dt)
{
	// A state gets one update in which it can see a finished animation before the sheet's
	// "next" chain is allowed to replace it.
	const bool wasFinished = m_animator.Finished();

	m_behavior.Update(*this, dt);

	if (wasFinished && m_animator.Finished() && !m_animator.Data().next.empty())
	{
		Play(m_animator.Data().next);
	}

	m_animator.Update(dt);

	invulnerable = invulnerable > dt ? invulnerable - dt : 0.0f;
	blink = blink > dt ? blink - dt : 0.0f;

	if (const char* state = StateName(); state != m_lastStateName)
	{
		const std::string message = name + ": " + state + "\n";
		OutputDebugStringA(message.c_str());
		m_lastStateName = state;
	}
}

void Actor::Draw(SpriteRenderer& sprites, int layer) const
{
	const Texture* texture = m_sheet->texture.get();
	const Rect frame = m_animator.Frame();
	if (texture == nullptr || frame.width <= 0 || frame.height <= 0)
	{
		return;
	}

	// Blinking: hidden for 50 ms out of every 100.
	const float blinkLeft = invulnerable > blink ? invulnerable : blink;
	if (blinkLeft > 0.0f && std::fmod(blinkLeft, 0.1f) < 0.05f)
	{
		return;
	}

	// The anchor is the bottom centre of the cell, rounded down to a whole pixel the way
	// ToAnchorRelative rounds it. Facing the other way mirrors the picture around that same
	// point, so the boxes and the picture stay lined up even on odd-width cells.
	const float anchorX = static_cast<float>(frame.width / 2);
	const float width = static_cast<float>(frame.width);

	Sprite sprite;
	sprite.texture = texture;
	sprite.texRect = frame;
	sprite.position = DrawPosition();	// whole pixels unless smoothed, so moving pixel art does not shimmer
	sprite.size = { width * scale, static_cast<float>(frame.height) * scale };
	sprite.flipX = Flipped();
	sprite.origin = { (sprite.flipX ? width - anchorX : anchorX) / width, 1.0f };
	sprite.smooth = smooth;
	sprite.layer = layer;
	sprites.Draw(sprite);

	// The rapier, in front of the body, turned about the hand: the grip behind the hand, the
	// guard across it, the blade out in front and its narrower tip at the end.
	XMFLOAT2 hand;
	float angle = 0.0f;
	bool strike = false;
	if (hasSword && CurrentHand(hand, angle, strike))
	{
		const XMFLOAT2 along = { std::cos(angle), std::sin(angle) };

		Sprite part;	// no texture: a plain rectangle
		part.position = hand;
		part.rotation = angle;
		part.layer = layer + 1;

		part.size = { c_gripLength, c_gripWidth };
		part.origin = { 1.0f, 0.5f };
		part.color = c_grip;
		sprites.Draw(part);

		part.size = { c_bladeLength - c_tipLength, c_bladeWidth };
		part.origin = { 0.0f, 0.5f };
		part.color = c_steel;
		sprites.Draw(part);

		part.position = { hand.x + along.x * (c_bladeLength - c_tipLength), hand.y + along.y * (c_bladeLength - c_tipLength) };
		part.size = { c_tipLength, c_tipWidth };
		sprites.Draw(part);

		part.position = hand;
		part.size = { c_guardWidth, c_guardLength };
		part.origin = { 0.5f, 0.5f };
		part.color = c_gold;
		sprites.Draw(part);
	}
}

void Actor::DrawDebug(SpriteRenderer& sprites, int layer) const
{
	auto outline = [&](const RectF& box, XMFLOAT4 color)
	{
		const float t = 0.5f;	// line thickness, in world units
		sprites.DrawRectangle({ box.x, box.y, box.width, t }, color, layer);
		sprites.DrawRectangle({ box.x, box.y + box.height - t, box.width, t }, color, layer);
		sprites.DrawRectangle({ box.x, box.y, t, box.height }, color, layer);
		sprites.DrawRectangle({ box.x + box.width - t, box.y, t, box.height }, color, layer);
	};

	outline(HurtBox(), { 0.0f, 200.0f / 255.0f, 1.0f, 1.0f });

	if (const auto hit = HitBox())
	{
		outline(*hit, { 1.0f, 60.0f / 255.0f, 60.0f / 255.0f, 1.0f });
	}
}
