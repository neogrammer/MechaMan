#include "pch.h"
#include "InputManager.h"

#include "GamePad.h"
#include "Keyboard.h"

using namespace MechaMan;

using DirectX::GamePad;
using DirectX::Keyboard;

namespace
{
	// Which keys do what. An action can have as many keys as you like, and a key can do more
	// than one thing (Enter is both Confirm and Start). Add a line to give an action a key.
	struct KeyBinding
	{
		Action			action;
		Keyboard::Keys	key;
	};

	const KeyBinding c_keyBindings[] =
	{
		{ Action::Left,		Keyboard::Keys::Left },
		{ Action::Left,		Keyboard::Keys::A },
		{ Action::Right,	Keyboard::Keys::Right },
		{ Action::Right,	Keyboard::Keys::D },
		{ Action::Up,		Keyboard::Keys::Up },
		{ Action::Up,		Keyboard::Keys::W },
		{ Action::Down,		Keyboard::Keys::Down },
		{ Action::Down,		Keyboard::Keys::S },

		{ Action::Jump,		Keyboard::Keys::Space },
		{ Action::Jump,		Keyboard::Keys::Z },
		{ Action::Jump,		Keyboard::Keys::K },
		{ Action::Shoot,	Keyboard::Keys::X },
		{ Action::Shoot,	Keyboard::Keys::J },

		{ Action::Confirm,	Keyboard::Keys::Enter },
		{ Action::Confirm,	Keyboard::Keys::Space },
		{ Action::Back,		Keyboard::Keys::Escape },
		{ Action::Back,		Keyboard::Keys::Back },		// Backspace
		{ Action::Start,	Keyboard::Keys::Enter },
		{ Action::Start,	Keyboard::Keys::Escape },
	};

	// Which controller buttons do what. A button is named by the GamePad::State function that
	// says whether it is pressed. The left stick counts as a direction once it is pushed more
	// than halfway; DirectXTK has already taken out the small drift near the middle.
	//
	// (The __cdecl is there because DirectXTK declares those functions with it. It changes
	// nothing in x64 and ARM64 builds, but Win32 builds need it to match.)
	using IsButtonPressed = bool (__cdecl GamePad::State::*)() const;

	struct ButtonBinding
	{
		Action			action;
		IsButtonPressed	isPressed;
	};

	const ButtonBinding c_buttonBindings[] =
	{
		{ Action::Left,		&GamePad::State::IsDPadLeftPressed },
		{ Action::Left,		&GamePad::State::IsLeftThumbStickLeft },
		{ Action::Right,	&GamePad::State::IsDPadRightPressed },
		{ Action::Right,	&GamePad::State::IsLeftThumbStickRight },
		{ Action::Up,		&GamePad::State::IsDPadUpPressed },
		{ Action::Up,		&GamePad::State::IsLeftThumbStickUp },
		{ Action::Down,		&GamePad::State::IsDPadDownPressed },
		{ Action::Down,		&GamePad::State::IsLeftThumbStickDown },

		{ Action::Jump,		&GamePad::State::IsAPressed },
		{ Action::Shoot,	&GamePad::State::IsXPressed },

		{ Action::Confirm,	&GamePad::State::IsAPressed },
		{ Action::Back,		&GamePad::State::IsBPressed },
		{ Action::Start,	&GamePad::State::IsMenuPressed },	// the small button with three lines
	};

	size_t Index(Action action)
	{
		return static_cast<size_t>(action);
	}
}

InputManager::InputManager() :
	m_keyboard(std::make_unique<Keyboard>()),
	m_gamePad(std::make_unique<GamePad>()),
	m_down{},
	m_wasDown{},
	m_controllerConnected(false)
{
	// App::Load creates MechaManMain on the window's own thread, so the window can be found
	// from here. The keyboard listens to it for key presses.
	Windows::UI::Core::CoreWindow^ window = Windows::UI::Core::CoreWindow::GetForCurrentThread();
	if (window == nullptr)
	{
		OutputDebugStringW(L"InputManager: there is no window on this thread, so the keyboard will not work.\n");
		return;
	}
	m_keyboard->SetWindow(window);

	// On Xbox, pressing B also asks the system to go back, and unless the app says it has
	// dealt with that, the system leaves the game and suspends it. Saying so keeps the player
	// in the game; the B button still reaches the game like any other button. The handler
	// holds on to nothing, so it can stay registered for as long as the app runs.
	Windows::UI::Core::SystemNavigationManager::GetForCurrentView()->BackRequested +=
		ref new Windows::Foundation::EventHandler<Windows::UI::Core::BackRequestedEventArgs^>(
			[](Platform::Object^, Windows::UI::Core::BackRequestedEventArgs^ args)
			{
				args->Handled = true;
			});
}

// Defined here rather than in the header, where Keyboard and GamePad are only names.
InputManager::~InputManager()
{
}

void InputManager::Update()
{
	std::array<bool, c_actionCount> down = {};

	const Keyboard::State keys = m_keyboard->GetState();
	for (const KeyBinding& binding : c_keyBindings)
	{
		if (keys.IsKeyDown(binding.key))
		{
			down[Index(binding.action)] = true;
		}
	}

	// Every connected controller counts, so the player can pick up whichever one is nearest.
	bool controllerConnected = false;
	for (int player = 0; player < GamePad::MAX_PLAYER_COUNT; player++)
	{
		const GamePad::State pad = m_gamePad->GetState(player);
		if (!pad.IsConnected())
		{
			continue;
		}

		controllerConnected = true;
		for (const ButtonBinding& binding : c_buttonBindings)
		{
			if ((pad.*binding.isPressed)())
			{
				down[Index(binding.action)] = true;
			}
		}
	}

	m_wasDown = m_down;
	m_down = down;
	m_controllerConnected = controllerConnected;
}

// DirectXTK lets go of the controllers while the app is suspended and looks for them again
// when it resumes, in case they were switched off or swapped in the meantime.
void InputManager::OnSuspending()
{
	m_gamePad->Suspend();
}

void InputManager::OnResuming()
{
	m_gamePad->Resume();
}

bool InputManager::IsDown(Action action) const
{
	return action < Action::Count && m_down[Index(action)];
}

bool InputManager::WasPressed(Action action) const
{
	return action < Action::Count && m_down[Index(action)] && !m_wasDown[Index(action)];
}

bool InputManager::WasReleased(Action action) const
{
	return action < Action::Count && !m_down[Index(action)] && m_wasDown[Index(action)];
}
