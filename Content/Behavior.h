#pragma once

// The current behavior of an actor: a std::variant of state structs and the dispatch around
// it. Generic on purpose: nothing in here names a state.
//
// Every state provides
//     void Enter(Actor&);
//     Next Update(Actor&, float dt);            Next = std::optional<StateVariant>
// and may provide
//     Next OnHit(Actor&, const HitEvent&);      left out = the state ignores hits
//
// A state never changes the variant it lives in (that would destroy the object whose member
// function is running). It returns the next state and Behavior swaps it in after the visit
// has returned.

#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace MechaMan
{
	class Actor;
	struct HitEvent;

	namespace BehaviorDetail
	{
		// True when S has OnHit(Actor&, const HitEvent&). (C++20 would write this as a
		// requires expression; this project is C++17, the newest C++/CX supports.)
		template <typename S, typename = void>
		struct HasOnHit : std::false_type {};

		template <typename S>
		struct HasOnHit<S, std::void_t<decltype(std::declval<S&>().OnHit(std::declval<Actor&>(), std::declval<const HitEvent&>()))>> : std::true_type {};
	}

	template <typename StateVariant>
	class Behavior
	{
	public:
		using Next = std::optional<StateVariant>;

		// Replace the current state with S built from args (in braces, so plain structs work),
		// then enter it.
		template <typename S, typename... Args>
		void Start(Actor& actor, Args&&... args)
		{
			m_state.template emplace<S>(S{ std::forward<Args>(args)... });
			std::get<S>(m_state).Enter(actor);
		}

		// Replace the current state with a ready-made one, then enter it.
		void Set(Actor& actor, StateVariant state)
		{
			m_state = std::move(state);
			std::visit([&](auto& s) { s.Enter(actor); }, m_state);
		}

		void Update(Actor& actor, float dt)
		{
			Apply(actor, std::visit([&](auto& s) -> Next { return s.Update(actor, dt); }, m_state));
		}

		void OnHit(Actor& actor, const HitEvent& hit)
		{
			Apply(actor, std::visit([&](auto& s) -> Next
				{
					using S = std::decay_t<decltype(s)>;
					if constexpr (BehaviorDetail::HasOnHit<S>::value)
					{
						return s.OnHit(actor, hit);
					}
					else
					{
						return std::nullopt;
					}
				}, m_state));
		}

		template <typename S>
		bool Is() const { return std::holds_alternative<S>(m_state); }

		const char* Name() const
		{
			return std::visit([](const auto& s) { return s.name; }, m_state);
		}

	private:
		void Apply(Actor& actor, Next next)
		{
			if (next)
			{
				Set(actor, std::move(*next));
			}
		}

		StateVariant m_state{};	// starts as the first alternative
	};
}
