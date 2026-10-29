#pragma once

#include <array>
#include <cstddef>
#include <memory>

namespace DirectX
{
	class GamePad;
	class Keyboard;
}

namespace MechaMan
{
	// What the player can do. Game code asks about these, never about particular keys or
	// buttons, so everything works the same on the keyboard and on a controller. Which keys
	// and buttons do what is set in two tables at the top of InputManager.cpp.
	//
	// To add an action, add it here (above Count), give it keys and buttons in those tables,
	// and give it a name in InputTestState.cpp.
	enum class Action
	{
		Left,
		Right,
		Up,
		Down,
		Jump,
		Shoot,
		Confirm,	// choose a menu item
		Back,		// leave a menu
		Start,		// pause, and PRESS START
		Count		// not an action: the number of actions above it
	};

	constexpr size_t c_actionCount = static_cast<size_t>(Action::Count);

	// Reads the keyboard and every connected controller once per update and turns what it
	// finds into actions. MechaManMain owns the one InputManager and hands it to each state's
	// constructor, the same way it hands out Graphics. A state keeps a reference
	// (InputManager& m_input) and asks it about actions from its Update:
	//
	//     if (m_input.WasPressed(Action::Jump)) { ... }	// once, in the update it went down
	//     if (m_input.IsDown(Action::Right)) { ... }		// in every update while it is held
	//
	// The answers stay the same for a whole update, so every state that updates in it sees
	// the same presses.
	class InputManager
	{
	public:
		InputManager();
		~InputManager();

		InputManager(const InputManager&) = delete;
		InputManager& operator=(const InputManager&) = delete;

		// MechaManMain calls these; states do not.
		void Update();			// once per game update, before the states update
		void OnSuspending();
		void OnResuming();

		// True while any key or button for the action is held down.
		bool IsDown(Action action) const;

		// True for one update only: the one in which the action went down, or came back up.
		// Holding a key down does not make it repeat.
		bool WasPressed(Action action) const;
		bool WasReleased(Action action) const;

		// True while at least one controller is connected.
		bool IsControllerConnected() const { return m_controllerConnected; }

	private:
		// DirectXTK's readers for the two devices.
		std::unique_ptr<DirectX::Keyboard>	m_keyboard;
		std::unique_ptr<DirectX::GamePad>	m_gamePad;

		// Which actions are down in this update, and which were down in the one before it.
		std::array<bool, c_actionCount>		m_down;
		std::array<bool, c_actionCount>		m_wasDown;

		bool								m_controllerConnected;
	};
}
