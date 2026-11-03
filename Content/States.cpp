#include "pch.h"
#include "States.h"

#include "Actor.h"

#include <algorithm>
#include <cmath>

using namespace MechaMan;

// Behavior finds OnHit on its own; these make the build fail if that ever stops working.
static_assert(BehaviorDetail::HasOnHit<Idle>::value, "Idle has OnHit, so Behavior must find it");
static_assert(!BehaviorDetail::HasOnHit<TakeHit>::value, "TakeHit has no OnHit, so Behavior must ignore hits");

namespace
{
	const float c_walkSpeed = 60.0f;			// enemy patrol speed, world units per second
	const float c_playerSpeed = 90.0f;
	const float c_sightRange = 160.0f;			// enemy notices the player
	const float c_attackRange = 40.0f;			// enemy swings
	const float c_attackCooldown = 0.5f;		// pause in Idle before the next swing
	const float c_idleBeforePatrol = 1.5f;
	const float c_guardHold = 0.6f;				// how long the enemy guards after seeing a swing
	const int c_guardBreakHits = 3;
	const float c_jumpSpeed = 330.0f;			// world units per second, straight up; about 60 units high with the World's gravity
	const float c_jumpCut = 120.0f;				// letting go of jump early slows the rise to this

	// Mega Man X's moves.
	const float c_dashSpeed = 210.0f;
	const float c_dashTime = 0.45f;				// the longest a dash lasts
	const float c_dashMinTime = 0.12f;			// a tap still dashes this long
	const float c_wallSlideSpeed = 70.0f;		// the fastest anyone slides down a wall
	const float c_wallKickSpeed = 300.0f;		// straight up, off a wall
	const float c_wallKickPush = 150.0f;		// away from the wall, while the stick is ignored
	const float c_wallKickLock = 0.12f;			// how long the stick is ignored after a kick

	// Being hit.
	const float c_knockback = 70.0f;			// pushed away from whatever hit, while flinching
	const float c_invulnerableTime = 1.0f;		// blinking and safe afterwards

	float DistanceTo(const Actor& a, const Actor& other)
	{
		return std::abs(other.position.x - a.position.x);
	}

	// True when x is on the side the actor is facing.
	bool InFront(const Actor& a, float x)
	{
		return a.FacingLeft() ? x < a.position.x : x > a.position.x;
	}

	// A point just in front of the actor.
	float AheadOf(const Actor& a)
	{
		return a.position.x + (a.FacingLeft() ? -1.0f : 1.0f);
	}

	const Actor* LiveTarget(const Actor& a)
	{
		return a.target && a.target->Alive() ? a.target : nullptr;
	}

	// Left and right in the air, at the speed the jump started with.
	void AirControl(Actor& a)
	{
		const float dir = (a.input.right ? 1.0f : 0.0f) - (a.input.left ? 1.0f : 0.0f);
		a.velocity.x = dir * a.airSpeed;

		if (dir != 0.0f)
		{
			a.Face(dir < 0.0f);
		}
	}

	// Touching a wall and pushing against it.
	bool PushingAgainstWall(const Actor& a)
	{
		return (a.wallLeft && a.input.left) || (a.wallRight && a.input.right);
	}

	// A jump press now or a moment ago (for the wall kick, which is easy to press early).
	bool JumpWanted(const Actor& a)
	{
		return a.input.jump || a.input.jumpBuffer > 0.0f;
	}
}

// ---------------------------------------------------------------------------
// Idle
// ---------------------------------------------------------------------------

void Idle::Enter(Actor& a)
{
	a.velocity.x = 0.0f;
	a.Play("idle");
}

Next Idle::Update(Actor& a, float dt)
{
	waited += dt;

	if (const Actor* t = LiveTarget(a))
	{
		const float d = DistanceTo(a, *t);

		if (d <= c_attackRange && t->Playing("attack"))
		{
			return Defend{ c_guardHold };				// they are swinging: guard first
		}

		if (d <= c_attackRange && waited >= c_attackCooldown)
		{
			return Attack{ t->position.x };
		}

		if (d <= c_sightRange)
		{
			a.Face(t->position.x < a.position.x);		// watch them (a Chase state would go here)
			return std::nullopt;
		}
	}

	if (waited >= c_idleBeforePatrol)
	{
		return Patrol{};
	}

	return std::nullopt;
}

Next Idle::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}

// ---------------------------------------------------------------------------
// Patrol
// ---------------------------------------------------------------------------

void Patrol::Enter(Actor& a)
{
	a.Play("walk");
}

Next Patrol::Update(Actor& a, float)
{
	if (const Actor* t = LiveTarget(a); t && DistanceTo(a, *t) <= c_sightRange)
	{
		return Idle{};
	}

	if (a.position.x <= a.homeLeft)
	{
		a.Face(false);
	}
	if (a.position.x >= a.homeRight)
	{
		a.Face(true);
	}

	a.velocity.x = a.FacingLeft() ? -c_walkSpeed : c_walkSpeed;
	return std::nullopt;
}

Next Patrol::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}

// ---------------------------------------------------------------------------
// Attack
// Which frames can hurt, and how far they reach, come from the hit lines in the .cfg
// (Actor::HitBox), so there is no timing logic here: face, play, wait.
// ---------------------------------------------------------------------------

void Attack::Enter(Actor& a)
{
	a.velocity.x = 0.0f;
	a.Face(targetX < a.position.x);
	a.Play("attack", false);
}

Next Attack::Update(Actor& a, float)
{
	if (!a.AnimationFinished())
	{
		return std::nullopt;
	}

	return a.rest;	// Idle for the AI (which has the cooldown), Player for the player
}

Next Attack::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };	// interruptible; return std::nullopt for super armor
}

// ---------------------------------------------------------------------------
// TakeHit
// ---------------------------------------------------------------------------

void TakeHit::Enter(Actor& a)
{
	a.health -= hit.damage;
	a.velocity.x = hit.fromX < a.position.x ? c_knockback : -c_knockback;
	a.invulnerable = c_invulnerableTime;
	a.Face(hit.fromX < a.position.x);
	a.Play("hurt", false);
}

Next TakeHit::Update(Actor& a, float)
{
	if (!a.AnimationFinished())
	{
		return std::nullopt;
	}

	a.velocity.x = 0.0f;

	if (!a.Alive())
	{
		return Dead{};
	}

	return a.rest;
}

// ---------------------------------------------------------------------------
// Defend
// ---------------------------------------------------------------------------

void Defend::Enter(Actor& a)
{
	a.velocity.x = 0.0f;

	if (const Actor* t = LiveTarget(a))
	{
		a.Face(t->position.x < a.position.x);
	}

	a.Play("defend");
}

Next Defend::Update(Actor& a, float dt)
{
	holdFor -= dt;

	// The AI guards for holdFor seconds; the player for as long as the button is held.
	if (holdFor <= 0.0f && !a.input.guard)
	{
		return a.rest;
	}

	return std::nullopt;
}

Next Defend::OnHit(Actor& a, const HitEvent& hit)
{
	if (!InFront(a, hit.fromX))
	{
		return TakeHit{ hit };
	}

	if (++blocked >= c_guardBreakHits)
	{
		return TakeHit{ hit };
	}

	a.health -= hit.damage / 4;		// chip damage

	if (!a.Alive())
	{
		return Dead{};
	}

	a.Play("defend_hit", false);	// its .cfg entry says "next defend", so it chains back
	return std::nullopt;
}

// ---------------------------------------------------------------------------
// Dead
// ---------------------------------------------------------------------------

void Dead::Enter(Actor& a)
{
	a.velocity.x = 0.0f;
	a.Play("dead", false);
}

Next Dead::Update(Actor& a, float)
{
	a.velocity.x = 0.0f;
	return std::nullopt;
}

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------

void Player::Enter(Actor& a)
{
	a.velocity.x = 0.0f;
	a.airSpeed = c_playerSpeed;
	a.Play("idle");
}

Next Player::Update(Actor& a, float)
{
	const ActorInput& in = a.input;

	// Back here in the air (after a swing or a flinch mid-jump, or walking off an edge):
	// keep falling.
	if (!a.onGround)
	{
		return Fall{};
	}

	if (in.jump)
	{
		return Jump{ in.dash };		// dash held: a dash jump, even from standing
	}

	if (in.dashPressed)
	{
		return Dash{};
	}

	if (in.attack)
	{
		return Attack{ AheadOf(a) };
	}

	if (in.crouch)
	{
		return Crouch{};
	}

	const float dir = (in.right ? 1.0f : 0.0f) - (in.left ? 1.0f : 0.0f);
	a.velocity.x = dir * c_playerSpeed;

	if (dir != 0.0f)
	{
		a.Face(dir < 0.0f);
	}

	const char* wanted = dir != 0.0f ? "walk" : "idle";
	if (!a.Playing(wanted))
	{
		a.Play(wanted);
	}

	return std::nullopt;
}

Next Player::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}

// ---------------------------------------------------------------------------
// Jump, Fall, Land
// The World pulls everyone down and stops them at the ground and walls (Actor::onGround,
// wallLeft, wallRight); these only start the jump, steer, and play the right animation.
// ---------------------------------------------------------------------------

void Jump::Enter(Actor& a)
{
	a.velocity.y = -c_jumpSpeed;
	a.airSpeed = fast ? c_dashSpeed : c_playerSpeed;
	a.onGround = false;
	a.Play("jump", false);
}

Next Jump::Update(Actor& a, float)
{
	AirControl(a);

	// Letting go of the button early makes a lower jump.
	if (!a.input.jumpHeld && a.velocity.y < -c_jumpCut)
	{
		a.velocity.y = -c_jumpCut;
	}

	if (a.input.attack)
	{
		return AirAttack{};
	}

	if (PushingAgainstWall(a) && a.input.jump)
	{
		return WallKick{};
	}

	if (a.velocity.y >= 0.0f)
	{
		return Fall{};
	}

	return std::nullopt;
}

Next Jump::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}

void Fall::Enter(Actor& a)
{
	a.Play("fall");
}

Next Fall::Update(Actor& a, float)
{
	AirControl(a);

	if (a.input.attack)
	{
		return AirAttack{};
	}

	if (a.onGround)
	{
		return Land{};
	}

	if (PushingAgainstWall(a))
	{
		if (JumpWanted(a))
		{
			return WallKick{};
		}

		if (a.velocity.y > 0.0f)
		{
			return WallSlide{};
		}
	}

	return std::nullopt;
}

Next Fall::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}

void Land::Enter(Actor& a)
{
	a.velocity.x = 0.0f;
	a.airSpeed = c_playerSpeed;
	a.Play("land", false);
}

Next Land::Update(Actor& a, float)
{
	if (!a.AnimationFinished())
	{
		return std::nullopt;
	}

	return a.rest;
}

Next Land::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}

// ---------------------------------------------------------------------------
// Dash
// ---------------------------------------------------------------------------

void Dash::Enter(Actor& a)
{
	a.Play("dash");
}

Next Dash::Update(Actor& a, float dt)
{
	time += dt;
	const ActorInput& in = a.input;

	// Off the end of a deck: keep the speed for the fall.
	if (!a.onGround)
	{
		a.airSpeed = c_dashSpeed;
		return Fall{};
	}

	if (in.jump)
	{
		return Jump{ true };
	}

	if (in.attack)
	{
		return Attack{ AheadOf(a) };
	}

	// Over when the time is up, when the button is let go (after a short minimum), into a
	// wall, or when the stick turns the other way.
	const bool turnedBack = a.FacingLeft() ? in.right : in.left;
	const bool intoWall = a.FacingLeft() ? a.wallLeft : a.wallRight;
	if (time >= c_dashTime || (!in.dash && time >= c_dashMinTime) || intoWall || turnedBack)
	{
		return Player{};
	}

	a.velocity.x = a.FacingLeft() ? -c_dashSpeed : c_dashSpeed;
	return std::nullopt;
}

Next Dash::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}

// ---------------------------------------------------------------------------
// WallSlide, WallKick
// ---------------------------------------------------------------------------

void WallSlide::Enter(Actor& a)
{
	a.Face(a.wallRight);	// back to the wall, ready to kick off
	a.Play("fall");
}

Next WallSlide::Update(Actor& a, float)
{
	if (a.onGround)
	{
		return Land{};
	}

	if (!PushingAgainstWall(a))
	{
		return Fall{};		// let go, or the wall ended
	}

	if (JumpWanted(a))
	{
		return WallKick{};
	}

	// Keep pressing into the wall so the contact holds, and slide slowly.
	a.velocity.x = a.wallLeft ? -1.0f : 1.0f;
	a.velocity.y = (std::min)(a.velocity.y, c_wallSlideSpeed);
	return std::nullopt;
}

Next WallSlide::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}

void WallKick::Enter(Actor& a)
{
	const bool wallOnLeft = a.wallLeft;

	a.input.jumpBuffer = 0.0f;		// this press is used up
	a.Face(!wallOnLeft);
	a.velocity.x = wallOnLeft ? c_wallKickPush : -c_wallKickPush;
	a.velocity.y = -c_wallKickSpeed;
	a.airSpeed = a.input.dash ? c_dashSpeed : c_playerSpeed;
	a.onGround = false;
	a.Play("flip", false);
}

Next WallKick::Update(Actor& a, float dt)
{
	time += dt;

	// First the push away from the wall, then steering again.
	if (time < c_wallKickLock)
	{
		return std::nullopt;
	}

	AirControl(a);

	if (a.input.attack)
	{
		return AirAttack{};
	}

	// Back at the wall on the way up: kick again, which is how a wall is climbed.
	if (PushingAgainstWall(a) && JumpWanted(a))
	{
		return WallKick{};
	}

	if (a.onGround)
	{
		return Land{};
	}

	if (a.velocity.y >= 0.0f)
	{
		return Fall{};
	}

	return std::nullopt;
}

Next WallKick::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}

// ---------------------------------------------------------------------------
// Crouch, CrouchAttack
// ---------------------------------------------------------------------------

void Crouch::Enter(Actor& a)
{
	a.velocity.x = 0.0f;
	a.Play(down ? "crouched" : "crouch_down");	// crouch_down's .cfg entry chains on to crouched
}

Next Crouch::Update(Actor& a, float)
{
	const ActorInput& in = a.input;
	a.velocity.x = 0.0f;

	if (!a.onGround)
	{
		return Fall{};
	}

	if (!in.crouch)
	{
		return Player{};
	}

	if (in.jump)
	{
		return Jump{ in.dash };
	}

	if (in.attack)
	{
		return CrouchAttack{};
	}

	// Left and right turn him around without standing up.
	if (in.left != in.right)
	{
		a.Face(in.left);
	}

	return std::nullopt;
}

Next Crouch::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}

void CrouchAttack::Enter(Actor& a)
{
	a.velocity.x = 0.0f;
	a.Play("kneel_attack", false);
}

Next CrouchAttack::Update(Actor& a, float)
{
	a.velocity.x = 0.0f;

	if (!a.onGround)
	{
		return Fall{};
	}

	if (!a.AnimationFinished())
	{
		return std::nullopt;
	}

	// Still holding Down: stay down, without getting down all over again.
	if (a.input.crouch)
	{
		return Crouch{ true };
	}

	return Player{};
}

Next CrouchAttack::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}

// ---------------------------------------------------------------------------
// AirAttack
// Nothing about the jump changes: gravity keeps pulling, the speed it started with carries on
// and the stick still steers. Only the facing is held, so the thrust stays pointed where it was
// aimed.
// ---------------------------------------------------------------------------

void AirAttack::Enter(Actor& a)
{
	a.Play("air_attack", false);
}

Next AirAttack::Update(Actor& a, float)
{
	const float dir = (a.input.right ? 1.0f : 0.0f) - (a.input.left ? 1.0f : 0.0f);
	a.velocity.x = dir * a.airSpeed;

	if (a.onGround)
	{
		return Land{};
	}

	if (a.AnimationFinished())
	{
		return Fall{};
	}

	return std::nullopt;
}

Next AirAttack::OnHit(Actor&, const HitEvent& hit)
{
	return TakeHit{ hit };
}
