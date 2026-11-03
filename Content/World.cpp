#include "pch.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

using namespace MechaMan;
using namespace DirectX;

namespace
{
	const float c_gravity = 900.0f;			// world units per second, per second, downwards
	const float c_maxFall = 420.0f;			// the fastest anything falls
	const float c_stepUp = 10.0f;			// a ledge this low is walked up onto rather than bumped into
	const float c_rampSnap = 6.0f;			// how far a slope reaches to keep feet on it
	const float c_fallOut = 48.0f;			// this far below the level, the player is lost
	const float c_jumpBuffer = 0.12f;		// a jump press is remembered this long, for wall kicks
	const float c_wakeMargin = 40.0f;		// enemies wake this far outside the view
	const float c_sleepDistance = 640.0f;	// and are not updated when this far from its middle
	const float c_contactGrace = 0.5f;		// after any hit, even a blocked one, the player is safe this long

	const float c_missileSpeed = 150.0f;
	const float c_missileLife = 4.0f;
	const int c_missileDamage = 15;
	const float c_mineGravity = 500.0f;
	const float c_mineFuse = 1.0f;			// seconds from landing to going off
	const float c_mineBlast = 24.0f;		// how far its explosion reaches
	const int c_mineDamage = 10;
	const float c_blastTime = 0.45f;		// how long an explosion shows

	// The factories are the only place that knows what kind of actor something is: same Actor
	// class, different sheet, facts and states.

	// Alucard is drawn a little smaller than his sheet, and smoothed rather than as sharp pixels
	// (which look uneven at a size that is not a whole number of them). His boxes and body
	// shrink with him; his sword keeps its length.
	const float c_playerScale = 0.8f;

	Actor MakePlayer(const SpriteSheet& sheet, XMFLOAT2 at)
	{
		Actor actor(sheet, "player");
		actor.scale = c_playerScale;
		actor.smooth = true;
		actor.position = at;
		actor.health = 100;
		actor.damage = 20;
		actor.hasSword = true;
		actor.body = { 18.0f * c_playerScale, 40.0f * c_playerScale };
		actor.rest = Player{};
		actor.GetBehavior().Start<Player>(actor);
		return actor;
	}

	// One of the stage's enemies, by the name of its sheet. Enemies sleep (inactive) until the
	// camera comes near.
	Actor MakeEnemy(const std::string& kind, const SpriteSheet& sheet, XMFLOAT2 at, int number)
	{
		Actor actor(sheet, kind + " " + std::to_string(number));
		actor.position = at;
		actor.homeLeft = at.x;
		actor.homeRight = at.x;
		actor.homeY = at.y;
		actor.active = false;

		auto flyer = [&actor]()
		{
			actor.collides = false;
			actor.gravity = 0.0f;
		};

		if (kind == "rider")				// Road Attacker: fast, so one well-timed swing is enough
		{
			actor.health = 20;
			actor.damage = 15;
			actor.body = { 40.0f, 26.0f };
			actor.GetBehavior().Start<Drive>(actor);
		}
		else if (kind == "bee1")			// Bomb Been
		{
			flyer();
			actor.health = 40;
			actor.damage = 10;
			actor.GetBehavior().Start<Bomber>(actor);
		}
		else if (kind == "bee2")			// the hover robot
		{
			flyer();
			actor.health = 60;
			actor.damage = 10;
			actor.GetBehavior().Start<Hover>(actor);
		}
		else if (kind == "bat")
		{
			flyer();
			actor.health = 20;
			actor.damage = 10;
			actor.GetBehavior().Start<Hang>(actor);
		}
		else if (kind == "elecmissle")		// Gun Volt
		{
			actor.health = 160;
			actor.damage = 20;
			actor.body = { 36.0f, 54.0f };
			actor.GetBehavior().Start<Turret>(actor);
		}
		else								// anything else: a swordsman, like the player
		{
			actor.health = 60;
			actor.damage = 10;
			actor.hasSword = true;
			actor.homeLeft = at.x - 100.0f;
			actor.homeRight = at.x + 100.0f;
			actor.body = { 18.0f, 40.0f };
			actor.rest = Idle{};
			actor.GetBehavior().Start<Patrol>(actor);
		}

		return actor;
	}

	XMFLOAT2 StartOf(const Stage& stage, size_t checkpoint)
	{
		if (stage.checkpoints.empty())
		{
			return { 64.0f, 64.0f };
		}
		return stage.checkpoints[(std::min)(checkpoint, stage.checkpoints.size() - 1)];
	}

	// True when the two boxes share some area; boxes that only touch do not overlap.
	bool Overlaps(const RectF& a, const RectF& b)
	{
		return a.x < b.x + b.width && b.x < a.x + a.width &&
			a.y < b.y + b.height && b.y < a.y + a.height;
	}

	// The box that meets the level: standing on the actor's position.
	RectF BodyBox(const Actor& a)
	{
		return { a.position.x - a.body.x * 0.5f, a.position.y - a.body.y, a.body.x, a.body.y };
	}

	XMFLOAT2 Centre(const RectF& box)
	{
		return { box.x + box.width * 0.5f, box.y + box.height * 0.5f };
	}

	XMFLOAT4 Color(int red, int green, int blue, float alpha = 1.0f)
	{
		return { red / 255.0f, green / 255.0f, blue / 255.0f, alpha };
	}

	void Outline(SpriteRenderer& sprites, const RectF& box, XMFLOAT4 color, int layer)
	{
		const float t = 0.5f;	// line thickness, in world units
		sprites.DrawRectangle({ box.x, box.y, box.width, t }, color, layer);
		sprites.DrawRectangle({ box.x, box.y + box.height - t, box.width, t }, color, layer);
		sprites.DrawRectangle({ box.x, box.y, t, box.height }, color, layer);
		sprites.DrawRectangle({ box.x + box.width - t, box.y, t, box.height }, color, layer);
	}
}

RectF Projectile::Box() const
{
	const Rect frame = animator.Frame();
	const float width = frame.width * 0.8f;
	const float height = frame.height * 0.8f;
	return { position.x - width * 0.5f, position.y - height * 0.5f, width, height };
}

World::World(const SpriteLibrary& sheets, const Stage& stage, size_t start) :
	m_stage(stage),
	m_missileSheet(sheets.Find("elecmissle")),
	m_mineSheet(sheets.Find("bee1")),
	m_player(MakePlayer(sheets.Get("alucard"), StartOf(stage, start)))
{
	m_enemies.reserve(stage.enemies.size());

	int number = 1;
	for (const EnemySpawn& spawn : stage.enemies)
	{
		const SpriteSheet* sheet = sheets.Find(spawn.kind);
		if (!sheet)
		{
			const std::string message = "World: no sheet for enemy kind '" + spawn.kind + "' (Assets\\cfg\\" + spawn.kind + ".cfg); left out\n";
			OutputDebugStringA(message.c_str());
			continue;
		}

		m_enemies.push_back(MakeEnemy(spawn.kind, *sheet, spawn.at, number++));
	}
}

// Walk with Left and Right (arrows, A and D, the D-pad or the left stick), jump with Jump
// (Space, Z or K, or the controller's A), attack with Shoot (J or X, or the controller's X),
// dash with Dash (Shift, C or L, or RB or RT), crouch while holding Down (S or the down
// arrow, or down on the D-pad or stick). Change the actions here to rebind; the keys behind
// each action are in InputManager.cpp.
void World::ReadInput(const InputManager& input)
{
	ActorInput& in = m_player.input;

	in.left = input.IsDown(Action::Left);
	in.right = input.IsDown(Action::Right);
	in.jump = input.WasPressed(Action::Jump);		// one jump per press
	in.jumpHeld = input.IsDown(Action::Jump);
	in.crouch = input.IsDown(Action::Down);
	in.attack = input.WasPressed(Action::Shoot);	// one thrust per press
	in.dash = input.IsDown(Action::Dash);
	in.dashPressed = input.WasPressed(Action::Dash);

	if (in.jump)
	{
		in.jumpBuffer = c_jumpBuffer;
	}
}

void World::Update(float dt)
{
	m_hits.clear();

	for (Actor& enemy : m_enemies)
	{
		enemy.target = &m_player;
	}

	WakeEnemies();

	Step(m_player, dt);

	const float viewMiddle = m_view.x + m_view.width * 0.5f;
	for (Actor& enemy : m_enemies)
	{
		if (enemy.active && !enemy.gone && std::abs(enemy.position.x - viewMiddle) < c_sleepDistance)
		{
			Step(enemy, dt);
		}
	}

	FireShots();
	UpdateProjectiles(dt);
	ResolveHits();
	RemoveDestroyed();

	m_projectiles.erase(std::remove_if(m_projectiles.begin(), m_projectiles.end(),
		[](const Projectile& p) { return p.done; }), m_projectiles.end());

	for (Blast& blast : m_blasts)
	{
		blast.age += dt;
	}
	m_blasts.erase(std::remove_if(m_blasts.begin(), m_blasts.end(),
		[](const Blast& b) { return b.age >= c_blastTime; }), m_blasts.end());

	// Fell out of the bottom of the level.
	if (m_player.Alive() && m_player.position.y > m_stage.size.y + c_fallOut)
	{
		m_player.health = 0;
		m_player.gravity = 0.0f;
		m_player.velocity = { 0.0f, 0.0f };
		m_player.GetBehavior().Set(m_player, Dead{});
	}

	ActorInput& in = m_player.input;
	in.jumpBuffer = in.jumpBuffer > dt ? in.jumpBuffer - dt : 0.0f;
}

// Gravity, then the actor's own behavior (which may set its velocity), then the move.
void World::Step(Actor& actor, float dt)
{
	if (actor.gravity > 0.0f)
	{
		actor.velocity.y = (std::min)(actor.velocity.y + c_gravity * actor.gravity * dt, c_maxFall);
	}

	actor.Update(dt);
	Move(actor, dt);
}

bool World::HitsSolid(const RectF& box) const
{
	for (const RectF& solid : m_stage.solids)
	{
		if (Overlaps(box, solid))
		{
			return true;
		}
	}
	return false;
}

// Something to stand on just under the point (x, y).
bool World::GroundAt(float x, float y) const
{
	if (HitsSolid({ x - 0.5f, y, 1.0f, 4.0f }))
	{
		return true;
	}

	for (const Ramp& ramp : m_stage.ramps)
	{
		if (x >= ramp.x0 && x <= ramp.x1 && std::abs(ramp.SurfaceAt(x) - y) < c_rampSnap)
		{
			return true;
		}
	}
	return false;
}

// Across first and then up and down, each time stopping at any solid in the way, so the body
// box never ends up inside one. Then the slopes, and the contact flags the states read.
void World::Move(Actor& a, float dt)
{
	if (!a.collides)
	{
		a.position.x += a.velocity.x * dt;
		a.position.y += a.velocity.y * dt;
		return;
	}

	const bool wasOnGround = a.onGround;

	// Across.
	const float dx = a.velocity.x * dt;
	if (dx != 0.0f)
	{
		a.position.x += dx;

		for (const RectF& solid : m_stage.solids)
		{
			if (!Overlaps(BodyBox(a), solid))
			{
				continue;
			}

			// A low ledge (the arena is a little higher than the deck before it), or the end
			// of a slope: step up onto it, if there is room.
			const float rise = a.position.y - solid.y;
			if (wasOnGround && rise > 0.0f && rise <= c_stepUp)
			{
				const float before = a.position.y;
				a.position.y = solid.y;
				if (!HitsSolid(BodyBox(a)))
				{
					continue;
				}
				a.position.y = before;
			}

			a.position.x = dx > 0.0f
				? solid.x - a.body.x * 0.5f
				: solid.x + solid.width + a.body.x * 0.5f;
			a.velocity.x = 0.0f;
		}
	}

	a.position.x = std::clamp(a.position.x, a.body.x * 0.5f, m_stage.size.x - a.body.x * 0.5f);

	// Up and down.
	const float dy = a.velocity.y * dt;
	a.position.y += dy;
	a.onGround = false;

	for (const RectF& solid : m_stage.solids)
	{
		if (!Overlaps(BodyBox(a), solid))
		{
			continue;
		}

		if (dy >= 0.0f)
		{
			a.position.y = solid.y;		// landed on it
			if (a.velocity.y > 0.0f)
			{
				a.velocity.y = 0.0f;
			}
			a.onGround = true;
		}
		else
		{
			a.position.y = solid.y + solid.height + a.body.y;	// head against its underside
			a.velocity.y = 0.0f;
		}
	}

	// Slopes are stood on from above only: landing on one, or walking along it (held to it
	// going downhill too).
	if (a.velocity.y >= 0.0f)
	{
		for (const Ramp& ramp : m_stage.ramps)
		{
			if (a.position.x < ramp.x0 || a.position.x > ramp.x1)
			{
				continue;
			}

			const float below = a.position.y - ramp.SurfaceAt(a.position.x);	// feet under the slope
			const float reachAbove = wasOnGround ? c_rampSnap : 0.0f;
			if (below >= -reachAbove && below <= (std::max)(dy, 0.0f) + c_rampSnap)
			{
				a.position.y = ramp.SurfaceAt(a.position.x);
				a.velocity.y = 0.0f;
				a.onGround = true;
			}
		}
	}

	// Standing still on something counts too.
	if (!a.onGround && a.velocity.y >= 0.0f)
	{
		a.onGround = GroundAt(a.position.x, a.position.y);
	}

	// What it is touching, for the states: walls on either side, and ground just ahead. A wall
	// is only a wall above the height of a step: where a slope meets a deck, the feet can sit
	// a hair below the deck's top, and that is not something to slide down or stop a dash.
	RectF box = BodyBox(a);
	box.height -= c_stepUp;
	box.x -= 1.0f;
	a.wallLeft = HitsSolid(box);
	box.x += 2.0f;
	a.wallRight = HitsSolid(box);

	const float ahead = a.position.x + (a.FacingLeft() ? -1.0f : 1.0f) * (a.body.x * 0.5f + 2.0f);
	a.groundAhead = GroundAt(ahead, a.position.y);
}

// Enemies wake as the view comes near them, and turn toward the player.
void World::WakeEnemies()
{
	for (Actor& enemy : m_enemies)
	{
		if (enemy.active || enemy.gone)
		{
			continue;
		}

		const bool across = enemy.position.x > m_view.x - c_wakeMargin && enemy.position.x < m_view.x + m_view.width + c_wakeMargin;
		const bool down = enemy.position.y > m_view.y - c_wakeMargin && enemy.position.y - enemy.body.y < m_view.y + m_view.height + c_wakeMargin;
		if (across && down)
		{
			enemy.active = true;
			enemy.Face(m_player.position.x < enemy.position.x);
		}
	}
}

// What the enemies asked for this step: Gun Volt's missiles and Bomb Been's mines.
void World::FireShots()
{
	for (Actor& enemy : m_enemies)
	{
		const ShotKind kind = enemy.shot;
		enemy.shot = ShotKind::None;

		Projectile p;
		p.kind = kind;

		if (kind == ShotKind::Missile && m_missileSheet && m_missileSheet->Has("missile"))
		{
			// Out of Gun Volt's front, at the height its launcher shows it.
			const float dir = enemy.FacingLeft() ? -1.0f : 1.0f;
			p.sheet = m_missileSheet;
			p.animator.Play(m_missileSheet->Get("missile"));
			p.position = { enemy.position.x + dir * 36.0f, enemy.position.y - 23.0f };
			p.velocity = { dir * c_missileSpeed, 0.0f };
			p.facingLeft = enemy.FacingLeft();
			p.damage = c_missileDamage;
		}
		else if (kind == ShotKind::Mine && m_mineSheet && m_mineSheet->Has("mine"))
		{
			// From under Bomb Been, drifting a little the way it flies.
			p.sheet = m_mineSheet;
			p.animator.Play(m_mineSheet->Get("mine"));
			p.position = { enemy.position.x, enemy.position.y + 2.0f };
			p.velocity = { enemy.velocity.x * 0.3f, 30.0f };
			p.damage = c_mineDamage;
		}
		else
		{
			continue;
		}

		m_projectiles.push_back(p);
	}
}

void World::UpdateProjectiles(float dt)
{
	for (Projectile& p : m_projectiles)
	{
		if (p.done)
		{
			continue;
		}

		p.age += dt;
		p.animator.Update(dt);

		if (p.kind == ShotKind::Missile)
		{
			p.position.x += p.velocity.x * dt;

			if (HitsSolid(p.Box()) || p.age > c_missileLife)
			{
				Burst(p, true);
			}
			continue;
		}

		// A mine: falls, lands, blinks for a moment and goes off.
		if (p.fuse < 0.0f)
		{
			p.velocity.y = (std::min)(p.velocity.y + c_mineGravity * dt, c_maxFall);
			p.position.x += p.velocity.x * dt;
			p.position.y += p.velocity.y * dt;

			for (const RectF& solid : m_stage.solids)
			{
				if (Overlaps(p.Box(), solid))
				{
					p.position.y = solid.y - p.Box().height * 0.5f;
					p.velocity = { 0.0f, 0.0f };
					p.fuse = c_mineFuse;
					break;
				}
			}

			if (p.position.y > m_stage.size.y + c_fallOut)
			{
				p.done = true;
			}
		}
		else
		{
			p.fuse -= dt;
			if (p.fuse <= 0.0f)
			{
				Burst(p, false);
			}
		}
	}
}

// A shot goes off. A mine's explosion hurts the player if they are close, unless it was set
// off harmlessly (by the sword, or by the hit it just did).
void World::Burst(Projectile& p, bool harmless)
{
	p.done = true;

	const bool mine = p.kind == ShotKind::Mine;
	m_blasts.push_back({ p.position, mine ? 26.0f : 16.0f });

	if (mine && !harmless)
	{
		const XMFLOAT2 centre = Centre(m_player.HurtBox());
		const float dx = centre.x - p.position.x;
		const float dy = centre.y - p.position.y;
		if (dx * dx + dy * dy < c_mineBlast * c_mineBlast)
		{
			HurtPlayer(p.damage, p.position.x);
		}
	}
}

// The player is hit by something other than the sword pass (a touch, a shot, an explosion).
// Their state decides what it costs (a guard only takes a chip); either way a moment's grace
// follows, so one touch is one hit.
void World::HurtPlayer(int damage, float fromX)
{
	if (!m_player.Alive() || m_player.invulnerable > 0.0f)
	{
		return;
	}

	const int before = m_player.health;
	m_player.Receive(HitEvent{ damage, fromX });
	Report(m_player, before - m_player.health);
	m_player.invulnerable = (std::max)(m_player.invulnerable, c_contactGrace);
}

void World::Report(const Actor& victim, int damage)
{
	if (damage > 0)
	{
		const RectF hurt = victim.HurtBox();
		m_hits.push_back({ { hurt.x + hurt.width * 0.5f, hurt.y }, damage });
	}
}

void World::ResolveHits()
{
	auto awake = [](const Actor& enemy) { return enemy.active && !enemy.gone && enemy.Alive(); };

	// The player's sword: each enemy once per swing. It also knocks shots out of the air.
	if (m_player.Alive())
	{
		if (const auto sword = m_player.HitBox())
		{
			for (Actor& enemy : m_enemies)
			{
				if (awake(enemy) && Overlaps(*sword, enemy.HurtBox()) && m_player.Strike(enemy))
				{
					const int before = enemy.health;
					enemy.Receive(HitEvent{ m_player.damage, m_player.position.x });
					Report(enemy, before - enemy.health);
				}
			}

			for (Projectile& p : m_projectiles)
			{
				if (!p.done && Overlaps(*sword, p.Box()))
				{
					Burst(p, true);
				}
			}
		}
	}

	// Swordsmen swing back.
	for (Actor& enemy : m_enemies)
	{
		if (!awake(enemy) || !enemy.hasSword)
		{
			continue;
		}

		const auto sword = enemy.HitBox();
		if (sword && Overlaps(*sword, m_player.HurtBox()) && enemy.Strike(m_player))
		{
			HurtPlayer(enemy.damage, enemy.position.x);
		}
	}

	// Touching an enemy hurts, Mega Man style.
	for (const Actor& enemy : m_enemies)
	{
		if (awake(enemy) && Overlaps(enemy.HurtBox(), m_player.HurtBox()))
		{
			HurtPlayer(enemy.damage, enemy.position.x);
			break;
		}
	}

	// And so does a shot, which goes off on the spot.
	for (Projectile& p : m_projectiles)
	{
		if (!p.done && Overlaps(p.Box(), m_player.HurtBox()))
		{
			HurtPlayer(p.damage, p.position.x);
			Burst(p, true);
		}
	}
}

// Destroyed enemies leave an explosion behind and are no longer updated or drawn.
void World::RemoveDestroyed()
{
	for (Actor& enemy : m_enemies)
	{
		if (enemy.gone && enemy.active)
		{
			const RectF box = enemy.HurtBox();
			m_blasts.push_back({ Centre(box), (std::max)(box.width, box.height) * 0.8f });
			enemy.active = false;
		}
	}
}

void World::Draw(SpriteRenderer& sprites, bool debugBoxes) const
{
	// The player on a layer above the enemies, so it is always drawn in front of them. Each
	// actor's sword goes on the layer above its body, so the layers step by two.
	for (const Actor& enemy : m_enemies)
	{
		if (enemy.active && !enemy.gone)
		{
			enemy.Draw(sprites, 0);
		}
	}

	m_player.Draw(sprites, 2);

	for (const Projectile& p : m_projectiles)
	{
		const Texture* texture = p.sheet ? p.sheet->texture.get() : nullptr;
		const Rect frame = p.animator.Frame();
		if (p.done || texture == nullptr || frame.width <= 0 || frame.height <= 0)
		{
			continue;
		}

		Sprite sprite;
		sprite.texture = texture;
		sprite.texRect = frame;
		sprite.position = { std::floor(p.position.x + 0.5f), std::floor(p.position.y + 0.5f) };
		sprite.origin = { 0.5f, 0.5f };
		sprite.flipX = p.facingLeft != p.sheet->facesLeft;
		sprite.layer = 4;
		sprites.Draw(sprite);
	}

	// Explosions: a hot core that swells and fades, and four puffs flying out of it.
	for (const Blast& b : m_blasts)
	{
		const float t = b.age / c_blastTime;
		const float fade = 1.0f - t;

		const float core = b.size * (0.4f + 0.8f * t);
		sprites.DrawRectangle({ b.at.x - core * 0.5f, b.at.y - core * 0.5f, core, core },
			{ 1.0f, 0.95f - 0.45f * t, 0.6f - 0.6f * t, fade }, 6);

		for (int i = 0; i < 4; i++)
		{
			const float angle = XM_PIDIV4 + i * XM_PIDIV2;
			const float distance = b.size * 0.9f * t;
			const float puff = b.size * 0.45f * (1.0f - 0.5f * t);
			sprites.DrawRectangle({ b.at.x + std::cos(angle) * distance - puff * 0.5f, b.at.y + std::sin(angle) * distance - puff * 0.5f, puff, puff },
				{ 1.0f, 0.55f, 0.15f, fade }, 5);
		}
	}

	if (debugBoxes)
	{
		for (const RectF& solid : m_stage.solids)
		{
			Outline(sprites, solid, Color(80, 255, 120), 99);
		}

		for (const Ramp& ramp : m_stage.ramps)
		{
			for (float x = ramp.x0; x <= ramp.x1; x += 3.0f)
			{
				sprites.DrawRectangle({ x, ramp.SurfaceAt(x) - 0.5f, 1.5f, 1.0f }, Color(255, 230, 80), 99);
			}
		}

		for (const Actor& enemy : m_enemies)
		{
			if (enemy.active && !enemy.gone)
			{
				enemy.DrawDebug(sprites);
			}
		}

		for (const Projectile& p : m_projectiles)
		{
			Outline(sprites, p.Box(), Color(255, 160, 60), 100);
		}

		m_player.DrawDebug(sprites);
		Outline(sprites, BodyBox(m_player), Color(255, 255, 255, 0.6f), 100);
	}
}
