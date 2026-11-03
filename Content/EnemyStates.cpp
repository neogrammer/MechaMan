#include "pch.h"
#include "States.h"

#include "Actor.h"

#include <algorithm>
#include <cmath>

// The intro stage's enemies. Their animation names are the ones in their .cfg files in
// Assets\cfg. None of them flinch: a hit takes health and makes them blink (Damaged), and at
// no health left they explode.

using namespace MechaMan;

static_assert(!BehaviorDetail::HasOnHit<Explode>::value, "Explode has no OnHit, so a destroyed enemy ignores hits");

namespace
{
	const float c_hitBlink = 0.25f;				// seconds an enemy blinks after a hit

	// Road Attacker.
	const float c_riderSpeed = 100.0f;

	// The hover robot.
	const float c_hoverSpeed = 70.0f;
	const float c_hoverAbove = 70.0f;			// how far above the player's feet it likes to be
	const float c_hoverAside = 55.0f;			// and how far to one side
	const float c_hoverSwitch = 3.0f;			// seconds before it moves over to the other side
	const float c_laughEvery = 4.5f;

	// Bomb Been.
	const float c_bomberSpeed = 65.0f;
	const float c_bomberPass = 150.0f;			// how far past the player it flies before turning back
	const float c_mineCooldown = 1.4f;
	const float c_mineReach = 18.0f;			// drops a mine when the player is this close below

	// The bat.
	const float c_batWakeRange = 100.0f;
	const float c_batSpeed = 80.0f;
	const float c_batSteer = 2.5f;				// how quickly it turns toward the player

	// Gun Volt.
	const float c_turretRange = 240.0f;
	const float c_turretHeight = 120.0f;		// ignores players far above or below it
	const float c_turretCooldown = 2.4f;
	const size_t c_missileFrame = 5;			// the shoot frame the missile leaves on

	const Actor* LiveTarget(const Actor& a)
	{
		return a.target && a.target->Alive() ? a.target : nullptr;
	}

	float Clamp(float value, float limit)
	{
		return (std::max)(-limit, (std::min)(limit, value));
	}
}

Next MechaMan::Damaged(Actor& a, const HitEvent& hit)
{
	a.health -= hit.damage;
	a.blink = c_hitBlink;

	if (!a.Alive())
	{
		return Explode{};
	}

	return std::nullopt;
}

// ---------------------------------------------------------------------------
// Road Attacker: Drive, Turn
// ---------------------------------------------------------------------------

void Drive::Enter(Actor& a)
{
	a.Play("move");
}

Next Drive::Update(Actor& a, float)
{
	const bool left = a.FacingLeft();
	const bool blocked = left ? a.wallLeft : a.wallRight;

	if (blocked || (a.onGround && !a.groundAhead))
	{
		return Turn{};
	}

	a.velocity.x = left ? -c_riderSpeed : c_riderSpeed;
	return std::nullopt;
}

Next Drive::OnHit(Actor& a, const HitEvent& hit)
{
	return Damaged(a, hit);
}

void Turn::Enter(Actor& a)
{
	a.velocity.x = 0.0f;
	a.Play("turn", false);
}

Next Turn::Update(Actor& a, float)
{
	a.velocity.x = 0.0f;

	if (!a.AnimationFinished())
	{
		return std::nullopt;
	}

	// The animation ends facing the other way, so the actor does too.
	a.Face(!a.FacingLeft());
	return Drive{};
}

Next Turn::OnHit(Actor& a, const HitEvent& hit)
{
	return Damaged(a, hit);
}

// ---------------------------------------------------------------------------
// The hover robot: Hover
// ---------------------------------------------------------------------------

void Hover::Enter(Actor& a)
{
	a.Play("fly");
}

Next Hover::Update(Actor& a, float dt)
{
	time += dt;

	DirectX::XMFLOAT2 goal = { a.homeLeft, a.homeY };
	const Actor* t = LiveTarget(a);
	if (t)
	{
		const float side = static_cast<int>(time / c_hoverSwitch) % 2 == 0 ? -1.0f : 1.0f;
		goal = { t->position.x + side * c_hoverAside, t->position.y - c_hoverAbove };
		a.Face(t->position.x < a.position.x);
	}
	goal.y += std::sin(time * 3.0f) * 8.0f;		// bobbing

	// Its .cfg chains the laugh back to fly. It hardly moves while it laughs.
	const bool laughing = a.Playing("laugh");
	const float speed = laughing ? c_hoverSpeed * 0.2f : c_hoverSpeed;
	a.velocity.x = Clamp((goal.x - a.position.x) * 1.5f, speed);
	a.velocity.y = Clamp((goal.y - a.position.y) * 2.0f, speed);

	if (t && !laughing && time >= nextLaugh)
	{
		a.Play("laugh");
		nextLaugh = time + c_laughEvery;
	}

	return std::nullopt;
}

Next Hover::OnHit(Actor& a, const HitEvent& hit)
{
	return Damaged(a, hit);
}

// ---------------------------------------------------------------------------
// Bomb Been: Bomber
// ---------------------------------------------------------------------------

void Bomber::Enter(Actor& a)
{
	a.Play("fly");
}

Next Bomber::Update(Actor& a, float dt)
{
	time += dt;
	cooldown -= dt;

	if (const Actor* t = LiveTarget(a))
	{
		// Well past the player: come back for another pass.
		const float past = a.FacingLeft() ? t->position.x - a.position.x : a.position.x - t->position.x;
		if (past > c_bomberPass)
		{
			a.Face(!a.FacingLeft());
		}

		// Right above them: drop a mine.
		if (cooldown <= 0.0f && std::abs(t->position.x - a.position.x) < c_mineReach && t->position.y > a.position.y)
		{
			a.shot = ShotKind::Mine;
			cooldown = c_mineCooldown;
		}
	}

	a.velocity.x = a.FacingLeft() ? -c_bomberSpeed : c_bomberSpeed;
	a.velocity.y = (a.homeY + std::sin(time * 2.0f) * 5.0f - a.position.y) * 2.0f;
	return std::nullopt;
}

Next Bomber::OnHit(Actor& a, const HitEvent& hit)
{
	return Damaged(a, hit);
}

// ---------------------------------------------------------------------------
// The bat: Hang, Swoop
// ---------------------------------------------------------------------------

void Hang::Enter(Actor& a)
{
	a.velocity = { 0.0f, 0.0f };
	a.Play("idle");
}

Next Hang::Update(Actor& a, float)
{
	a.velocity = { 0.0f, 0.0f };

	const Actor* t = LiveTarget(a);
	if (t && std::abs(t->position.x - a.position.x) < c_batWakeRange && t->position.y > a.position.y - 16.0f)
	{
		return Swoop{};
	}

	return std::nullopt;
}

Next Hang::OnHit(Actor& a, const HitEvent& hit)
{
	return Damaged(a, hit);
}

void Swoop::Enter(Actor& a)
{
	a.velocity = { 0.0f, 0.0f };
	a.Play("wake");		// its .cfg chains it to fly
}

Next Swoop::Update(Actor& a, float dt)
{
	time += dt;

	if (a.Playing("wake"))
	{
		a.velocity = { 0.0f, 0.0f };	// opening its eyes
		return std::nullopt;
	}

	const Actor* t = LiveTarget(a);
	if (!t)
	{
		return std::nullopt;
	}

	// Steer toward the middle of the player's body, with a wobble.
	const float dx = t->position.x - a.position.x;
	const float dy = (t->position.y - 20.0f) - a.position.y + std::sin(time * 5.0f) * 12.0f;
	const float length = (std::max)(1.0f, std::sqrt(dx * dx + dy * dy));
	const float k = (std::min)(1.0f, c_batSteer * dt);

	a.velocity.x += (dx / length * c_batSpeed - a.velocity.x) * k;
	a.velocity.y += (dy / length * c_batSpeed - a.velocity.y) * k;
	a.Face(a.velocity.x < 0.0f);
	return std::nullopt;
}

Next Swoop::OnHit(Actor& a, const HitEvent& hit)
{
	return Damaged(a, hit);
}

// ---------------------------------------------------------------------------
// Gun Volt: Turret, Fire
// ---------------------------------------------------------------------------

void Turret::Enter(Actor& a)
{
	a.velocity.x = 0.0f;
	a.Play("idle");
}

Next Turret::Update(Actor& a, float dt)
{
	a.velocity.x = 0.0f;
	cooldown -= dt;

	const Actor* t = LiveTarget(a);
	if (!t || std::abs(t->position.x - a.position.x) > c_turretRange || std::abs(t->position.y - a.position.y) > c_turretHeight)
	{
		return std::nullopt;
	}

	a.Face(t->position.x < a.position.x);

	if (cooldown <= 0.0f)
	{
		return Fire{};
	}

	return std::nullopt;
}

Next Turret::OnHit(Actor& a, const HitEvent& hit)
{
	return Damaged(a, hit);
}

void Fire::Enter(Actor& a)
{
	a.velocity.x = 0.0f;
	a.Play("readyshot", false);
}

Next Fire::Update(Actor& a, float)
{
	a.velocity.x = 0.0f;

	if (a.Playing("readyshot"))
	{
		if (a.AnimationFinished())
		{
			a.Play("shoot", false);
		}
		return std::nullopt;
	}

	if (!fired && (a.FrameIndex() >= c_missileFrame || a.AnimationFinished()))
	{
		a.shot = ShotKind::Missile;
		fired = true;
	}

	if (a.AnimationFinished())
	{
		return Turret{ c_turretCooldown };
	}

	return std::nullopt;
}

Next Fire::OnHit(Actor& a, const HitEvent& hit)
{
	return Damaged(a, hit);
}

// ---------------------------------------------------------------------------
// Explode
// ---------------------------------------------------------------------------

void Explode::Enter(Actor& a)
{
	a.velocity = { 0.0f, 0.0f };
	a.gone = true;
}

Next Explode::Update(Actor& a, float)
{
	a.velocity = { 0.0f, 0.0f };
	return std::nullopt;
}
