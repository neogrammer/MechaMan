#pragma once

// One animated thing in the world. Generic: it does not know what an "Alucard" is. A sheet
// gives it its looks, a rest state and a starting state give it its behavior (see the
// factories in World.cpp).
//
// Positions and boxes are in world units: one unit is one pixel of the sheet, and the World
// decides how much of the world fills the screen.

#include "Behavior.h"
#include "SpriteSheet.h"
#include "States.h"

#include <optional>
#include <string>
#include <vector>

namespace MechaMan
{
	// A projectile a state wants fired; the World fires it and sets the request back to None.
	enum class ShotKind
	{
		None,
		Missile,	// Gun Volt's, straight ahead
		Mine		// Bomb Been's, dropped
	};

	class Actor
	{
	public:
		// The sheet must outlive the actor (keep sheets in a SpriteLibrary).
		explicit Actor(const SpriteSheet& sheet, std::string name = "actor");

		// Facts the states read and write. Anything that must survive a state change lives
		// here, never inside a state.
		std::string name;							// for the debugger's Output window
		float scale = 1.0f;							// world units per pixel of the sheet: its picture, boxes and hands (a sword keeps its own length)
		bool smooth = false;						// its picture smoothed instead of drawn as sharp pixels
		DirectX::XMFLOAT2 position = { 0.0f, 0.0f };	// the anchor: bottom centre of the sprite (the feet)
		DirectX::XMFLOAT2 velocity = { 0.0f, 0.0f };	// the World moves the actor by it after Update
		float homeLeft = 0.0f;						// Patrol bounds
		float homeRight = 0.0f;
		float homeY = 0.0f;							// a flyer's cruising height
		int health = 100;
		int damage = 10;							// what its sword does, or touching it does
		const Actor* target = nullptr;				// set by the world every step; may be null
		ActorInput input;							// set by the world every step (player only)
		States rest = Idle{};						// where one-shot states (Attack, TakeHit, Defend) return to
		bool hasSword = false;						// a sword in the hand on frames with a hand line, hitting with its blade

		// Moving through the level. The World applies gravity and velocity and stops the body
		// box at the stage's solids, then sets the contact flags.
		DirectX::XMFLOAT2 body = { 16.0f, 40.0f };	// width and height of the box that meets the level, standing on position
		bool collides = true;						// false: flies through the level
		float gravity = 1.0f;						// how much of the World's gravity pulls on it; 0 for flyers
		float airSpeed = 90.0f;						// left and right speed in the air; a dash jump raises it
		bool onGround = true;						// standing on something; set by the world every step
		bool wallLeft = false;						// touching a solid on that side; set by the world every step
		bool wallRight = false;
		bool groundAhead = true;					// there is ground just in front of its feet; set by the world

		// Being hit, and being destroyed.
		float invulnerable = 0.0f;					// seconds left in which hits are ignored; it blinks meanwhile
		float blink = 0.0f;							// seconds left of an enemy's hit flash
		bool active = true;							// updated and drawn; the World wakes enemies when the camera comes near
		bool gone = false;							// destroyed: the World takes it out with an explosion
		ShotKind shot = ShotKind::None;				// a projectile the state wants fired this step

		bool FacingLeft() const { return m_facingLeft; }
		void Face(bool left) { m_facingLeft = left; }
		bool Alive() const { return health > 0; }

		// Always restarts the animation, even when it is already playing. States only call
		// these on transitions, which is what makes that safe. A name the sheet does not have
		// plays the sheet's first animation and logs once.
		void Play(const std::string& animation);			// loop flag from the .cfg
		void Play(const std::string& animation, bool loop);
		bool Playing(const std::string& animation) const { return m_animationName == animation; }
		bool AnimationFinished() const { return m_animator.Finished(); }
		size_t FrameIndex() const { return m_animator.Index(); }

		// For the world's hit pass. World units, mirrored when facing left. The hit box is the
		// sword's blade on frames whose hand line says strike, and otherwise the frame's hit line.
		RectF HurtBox() const;
		std::optional<RectF> HitBox() const;
		bool Strike(const Actor& victim);					// true the first time this swing reaches victim
		void Receive(const HitEvent& hit) { m_behavior.OnHit(*this, hit); }

		Behavior<States>& GetBehavior() { return m_behavior; }
		template <typename S> bool Is() const { return m_behavior.Is<S>(); }
		const char* StateName() const { return m_behavior.Name(); }

		// The behavior, the animation and the timers. It does not move the actor: the World
		// does that afterwards, so it can stop the actor at walls and floors.
		void Update(float dt);

		// Between the SpriteRenderer's Begin and End. Nothing is drawn while the sheet has no
		// texture (before CreateDeviceDependentResources, or after the device is lost), or on
		// the off beats of a blink.
		void Draw(SpriteRenderer& sprites, int layer = 0) const;
		void DrawDebug(SpriteRenderer& sprites, int layer = 100) const;	// hurt box in blue, hit box in red

	private:
		bool Flipped() const { return m_facingLeft != m_sheet->facesLeft; }
		RectF ToWorld(const Rect& box) const;

		// Where the actor is drawn: its position, snapped to whole pixels unless it is smoothed
		// (a smoothed picture moves evenly in between them).
		DirectX::XMFLOAT2 DrawPosition() const;

		// The current frame's hand pose in the world, facing the way the actor faces. False when
		// the frame has no hand line.
		bool CurrentHand(DirectX::XMFLOAT2& hand, float& angle, bool& strike) const;

		const SpriteSheet*		m_sheet;
		Animator				m_animator;
		Behavior<States>		m_behavior;
		std::string				m_animationName;
		const char*				m_lastStateName = nullptr;
		bool					m_facingLeft = false;
		std::vector<const Actor*>	m_struck;		// victims this swing has already reached
	};
}
