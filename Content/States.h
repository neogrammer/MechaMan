#pragma once

// The behaviors. A state holds only what lives as long as the state (a timer, the hit that
// caused it). Anything that must survive a transition (health, patrol bounds, the target)
// lives on the Actor.
//
// Bodies are in States.cpp (the player and the swordsmen) and EnemyStates.cpp (the intro
// stage's enemies): a state's function can only mention other states once every state is
// complete, so they cannot be defined inline here.
//
// The states are plain structs, so they are built with their fields in order:
// Attack{ targetX }, Defend{ holdFor }, TakeHit{ hit }, Jump{ fast }, Turret{ cooldown }.

#include <optional>
#include <variant>

namespace MechaMan
{
	class Actor;

	struct HitEvent
	{
		int damage = 0;
		float fromX = 0.0f;		// attacker's x, so the victim can face it and test front or back
	};

	// What the controls want this step. Only the player's Actor gets one filled in (by
	// World::ReadInput, from the InputManager).
	struct ActorInput
	{
		bool left = false;
		bool right = false;
		bool jump = false;			// true only on the step the button went down
		bool jumpHeld = false;		// true while held: letting go early makes a lower jump
		float jumpBuffer = 0.0f;	// seconds since a recent press, so a wall kick pressed a moment early still happens
		bool attack = false;		// true only on the step the button went down
		bool crouch = false;		// true while held
		bool guard = false;			// true while held (the swordsmen's guard; the player's Down crouches instead)
		bool dash = false;			// true while held
		bool dashPressed = false;	// true only on the step the button went down
	};

	// The player and the swordsmen.
	struct Idle;
	struct Patrol;
	struct Attack;
	struct TakeHit;
	struct Defend;
	struct Dead;
	struct Player;
	struct Jump;
	struct Fall;
	struct Land;
	struct Dash;
	struct WallSlide;
	struct WallKick;
	struct Crouch;
	struct CrouchAttack;
	struct AirAttack;

	// The intro stage's enemies.
	struct Drive;
	struct Turn;
	struct Hover;
	struct Bomber;
	struct Hang;
	struct Swoop;
	struct Turret;
	struct Fire;
	struct Explode;

	// Idle first: it is what a fresh Behavior holds.
	using States = std::variant<Idle, Patrol, Attack, TakeHit, Defend, Dead, Player, Jump, Fall, Land, Dash, WallSlide, WallKick,
		Crouch, CrouchAttack, AirAttack,
		Drive, Turn, Hover, Bomber, Hang, Swoop, Turret, Fire, Explode>;
	using Next = std::optional<States>;		// nullopt = stay in the current state

	// A small enemy takes a hit: it loses the health and blinks, and at none left it explodes.
	// The enemies' OnHit functions all come here; they do not flinch.
	Next Damaged(Actor& a, const HitEvent& hit);

	// -----------------------------------------------------------------------------------------
	// The player and the swordsmen
	// -----------------------------------------------------------------------------------------

	// Stand still, watch for a target, wander off after a while. The AI's hub.
	struct Idle
	{
		static constexpr const char* name = "Idle";
		float waited = 0.0f;			// seconds in this state

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Walk between the actor's homeLeft and homeRight.
	struct Patrol
	{
		static constexpr const char* name = "Patrol";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Commit to one swing, then return to the actor's rest state.
	struct Attack
	{
		static constexpr const char* name = "Attack";
		float targetX = 0.0f;			// where the target was when we committed

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Take the damage, get knocked back a little, blink and ignore further hits for a moment.
	struct TakeHit
	{
		static constexpr const char* name = "TakeHit";
		HitEvent hit;

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		// no OnHit: invulnerable while flinching
	};

	// Hold the guard: front hits are absorbed, back hits land, too many blocks break it.
	struct Defend
	{
		static constexpr const char* name = "Defend";
		float holdFor = 1.0f;			// seconds to keep the guard up (the player also holds the button)
		int blocked = 0;				// hits absorbed so far

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Play the death animation and stay there.
	struct Dead
	{
		static constexpr const char* name = "Dead";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		// no OnHit
	};

	// Controller or keyboard control: walk, jump, dash, attack, guard. The player's rest state.
	struct Player
	{
		static constexpr const char* name = "Player";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Leave the ground. Steerable left and right, can attack, can kick off a wall; Fall takes
	// over at the top. A fast jump (dash held) keeps the dash's speed in the air.
	struct Jump
	{
		static constexpr const char* name = "Jump";
		bool fast = false;

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// In the air on the way down (from a jump, or off anything else), until the ground.
	// Pushing against a wall on the way down slides down it.
	struct Fall
	{
		static constexpr const char* name = "Fall";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Touch down, then back to the rest state.
	struct Land
	{
		static constexpr const char* name = "Land";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// A burst along the ground, for as long as the button is held (up to a limit). Jumping out
	// of it keeps its speed in the air: the dash jump.
	struct Dash
	{
		static constexpr const char* name = "Dash";
		float time = 0.0f;

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Pushing against a wall in the air: slide down it slowly, and jump to kick off.
	struct WallSlide
	{
		static constexpr const char* name = "WallSlide";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Kick off a wall: up and away from it, without steering for a moment, then steer back to
	// the wall and kick again to climb it.
	struct WallKick
	{
		static constexpr const char* name = "WallKick";
		float time = 0.0f;

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Holding Down: get down and stay down. Left and right turn around without standing up;
	// attack thrusts low, jump jumps. 'down' skips getting down (after a crouching thrust).
	struct Crouch
	{
		static constexpr const char* name = "Crouch";
		bool down = false;

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// The thrust from a crouch, then back down (or up, if Down was let go).
	struct CrouchAttack
	{
		static constexpr const char* name = "CrouchAttack";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// The thrust in the air. He carries on flying the way he was going, still steering, but
	// facing the way he thrust; then he falls as usual.
	struct AirAttack
	{
		static constexpr const char* name = "AirAttack";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// -----------------------------------------------------------------------------------------
	// The intro stage's enemies (EnemyStates.cpp). They wake when the camera comes near (the
	// World sets Actor::active); until then they are not updated.
	// -----------------------------------------------------------------------------------------

	// Road Attacker: drive along the deck, turn around at walls and edges.
	struct Drive
	{
		static constexpr const char* name = "Drive";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	struct Turn
	{
		static constexpr const char* name = "Turn";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// The purple hover robot: keep above and to one side of the player, bobbing, switching
	// sides now and then, and stopping to laugh.
	struct Hover
	{
		static constexpr const char* name = "Hover";
		float time = 0.0f;
		float nextLaugh = 3.0f;

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Bomb Been: fly across at its height, drop a mine on whoever is below, turn back after
	// passing them.
	struct Bomber
	{
		static constexpr const char* name = "Bomber";
		float cooldown = 0.0f;
		float time = 0.0f;

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// The bat: hang wrapped in its wings until someone comes close.
	struct Hang
	{
		static constexpr const char* name = "Hang";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// The bat, awake: open its eyes, then fly at the player, wobbling.
	struct Swoop
	{
		static constexpr const char* name = "Swoop";
		float time = 0.0f;

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Gun Volt: stand and face the player, and fire when the cooldown is over and they are
	// in range.
	struct Turret
	{
		static constexpr const char* name = "Turret";
		float cooldown = 1.0f;

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Gun Volt: open up, then fire a missile.
	struct Fire
	{
		static constexpr const char* name = "Fire";
		bool fired = false;

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		Next OnHit(Actor& a, const HitEvent& hit);
	};

	// Destroyed: the World sees Actor::gone, puts an explosion where it was and takes it out.
	struct Explode
	{
		static constexpr const char* name = "Explode";

		void Enter(Actor& a);
		Next Update(Actor& a, float dt);
		// no OnHit
	};
}
