#pragma once

// The player, the enemies and their shots, moving through a Stage. Owns the rules that are
// between actors rather than inside one: gravity, walls and floors, waking enemies as the
// camera comes near, who hits whom, explosions, falling out of the level.

#include "Actor.h"
#include "InputManager.h"
#include "SpriteRenderer.h"
#include "SpriteSheet.h"
#include "Stage.h"

#include <vector>

namespace MechaMan
{
	// A hit that landed: how much health it took, and where (the top middle of the victim's
	// hurt box, world units), for damage numbers and the like.
	struct HitReport
	{
		DirectX::XMFLOAT2	at;
		int					damage;
	};

	// A missile or a mine on its way.
	struct Projectile
	{
		ShotKind				kind = ShotKind::None;
		const SpriteSheet*		sheet = nullptr;
		Animator				animator;
		DirectX::XMFLOAT2		position = { 0.0f, 0.0f };	// the centre
		DirectX::XMFLOAT2		velocity = { 0.0f, 0.0f };
		bool					facingLeft = false;
		int						damage = 10;
		float					age = 0.0f;
		float					fuse = -1.0f;		// a mine: seconds left once it has landed
		bool					done = false;

		RectF Box() const;
	};

	// An explosion, drawn as flashes that grow and fade.
	struct Blast
	{
		DirectX::XMFLOAT2	at;
		float				size;
		float				age = 0.0f;
	};

	class World
	{
	public:
		// The player starts at the stage's checkpoint number 'start' (the last one if there are
		// fewer). The sheets must already hold "alucard" and one sheet per enemy kind the stage
		// uses (rider, bee1, ...); an enemy whose sheet is missing is left out. The library and
		// the stage must outlive the world.
		World(const SpriteLibrary& sheets, const Stage& stage, size_t start);

		World(const World&) = delete;				// the actors point at each other
		World& operator=(const World&) = delete;

		// The controls into the player's ActorInput. Call once per update, before Update.
		void ReadInput(const InputManager& input);

		// The part of the level on screen, world units: enemies wake as it comes near them.
		void SetView(const RectF& view) { m_view = view; }

		void Update(float dt);

		// The actors, shots and explosions, in world units, inside a batch that is already open:
		// the Scene's action layer. With debugBoxes, also the stage's solids and everyone's boxes.
		void Draw(SpriteRenderer& sprites, bool debugBoxes) const;

		DirectX::XMFLOAT2 Size() const { return m_stage.size; }
		const Actor& GetPlayer() const { return m_player; }
		const std::vector<Actor>& Enemies() const { return m_enemies; }

		// The hits that landed in the last Update.
		const std::vector<HitReport>& Hits() const { return m_hits; }

	private:
		void Step(Actor& actor, float dt);
		void Move(Actor& actor, float dt);
		bool HitsSolid(const RectF& box) const;
		bool GroundAt(float x, float y) const;

		void WakeEnemies();
		void FireShots();
		void UpdateProjectiles(float dt);
		void ResolveHits();
		void RemoveDestroyed();
		void Burst(Projectile& projectile, bool harmless);
		void HurtPlayer(int damage, float fromX);
		void Report(const Actor& victim, int damage);

		const Stage&			m_stage;
		const SpriteSheet*		m_missileSheet = nullptr;	// Gun Volt's sheet holds its missile
		const SpriteSheet*		m_mineSheet = nullptr;		// and Bomb Been's its mine
		RectF					m_view = { 0.0f, 0.0f, 320.0f, 180.0f };

		Actor					m_player;
		std::vector<Actor>		m_enemies;		// never resized after construction, so pointers to actors stay valid
		std::vector<Projectile>	m_projectiles;
		std::vector<Blast>		m_blasts;
		std::vector<HitReport>	m_hits;
	};
}
