#pragma once

// Loads a .cfg written by the Sprite Sheet Processor and hands out its animations. Also holds
// Animator (plays one animation) and SpriteLibrary (keeps every loaded sheet at a stable
// address).
//
// File format, one animation per row of the image:
//
//   image <file, relative to this config>   ../ climbs out of the config's folder
//   facing left | right              which way the artwork faces (default right)
//   anim <name> <frames> <frameWidth> <frameHeight> <rowY> <secondsPerFrame> [firstFrame]
//                                    firstFrame: the column the animation starts at (default 0)
//     loop | once                    repeat (default) or stop on the last frame
//     next <anim>                    animation to play after a "once" animation ends
//     hurt <x> <y> <w> <h>           body box, cell coordinates, one per animation
//     hit <i> <x> <y> <w> <h>        damage box on frame i, cell coordinates
//     hand <i> <x> <y> <angle> [strike]
//                                    where the hand is on frame i (cell coordinates) and which way a
//                                    held sword points (degrees: 0 forward, negative up); strike means
//                                    the blade can hurt on that frame
//
// Lines after an "anim" line belong to that animation. Frame i of an animation is at
// x = (firstFrame + i) * frameWidth, y = rowY. Boxes are converted to anchor-relative coordinates on load
// (see AnimationData.h).
//
// PC and Xbox alike: the .cfg and its image are read from the app package, so both have to be
// in the project with Content set to Yes for All Configurations (images added as Image items
// already are). File names are package-relative, for example L"Assets\\alucard.cfg". Problems
// are written to the debugger's Output window.
//
// The animations are plain data, loaded once. The image is a GPU texture, so it is created and
// released with the device: call CreateDeviceDependentResources and
// ReleaseDeviceDependentResources from the owning state's functions of the same names.

#include "AnimationData.h"
#include "SpriteRenderer.h"

#include <cstddef>
#include <map>
#include <memory>
#include <string>

namespace MechaMan
{
	class SpriteSheet
	{
	public:
		std::shared_ptr<Texture> texture;		// null until CreateDeviceDependentResources
		std::wstring imageFile;					// package-relative
		bool facesLeft = false;

		// std::map: nodes never move, so pointers into it stay valid.
		std::map<std::string, AnimationData> animations;

		bool Load(const std::wstring& configFile);

		void CreateDeviceDependentResources(SpriteRenderer& sprites);
		void ReleaseDeviceDependentResources() { texture = nullptr; }

		bool Has(const std::string& name) const { return animations.count(name) != 0; }

		const AnimationData* Find(const std::string& name) const
		{
			const auto it = animations.find(name);
			return it == animations.end() ? nullptr : &it->second;
		}

		// Throws std::out_of_range if the name is not in the file.
		const AnimationData& Get(const std::string& name) const { return animations.at(name); }

		// Only valid after a successful Load (Load fails on a sheet with no animations).
		const AnimationData& First() const { return animations.begin()->second; }
	};

	// Plays one animation. Call Update every step, then use Frame (for Sprite::SetTexRect).
	class Animator
	{
	public:
		// Starts data from its first frame, even when it is already playing.
		void Play(const AnimationData& data, bool loop)
		{
			m_data = &data;
			m_loop = loop;
			m_index = 0;
			m_time = 0.0f;
			m_done = false;
		}

		void Play(const AnimationData& data) { Play(data, data.loop); }

		void Update(float dt)
		{
			if (!m_data || m_data->frames.empty() || m_data->secondsPerFrame <= 0.0f || m_done)
			{
				return;
			}

			m_time += dt;

			while (m_time >= m_data->secondsPerFrame)
			{
				m_time -= m_data->secondsPerFrame;

				if (m_index + 1 < m_data->frames.size())
				{
					++m_index;
				}
				else if (m_loop)
				{
					m_index = 0;
				}
				else
				{
					// The last frame has been shown for its full duration.
					m_done = true;
					m_time = 0.0f;
					break;
				}
			}
		}

		bool Playing() const { return m_data != nullptr; }

		// Only valid when Playing().
		const AnimationData& Data() const { return *m_data; }

		size_t Index() const { return m_index; }

		// The texRect of the current frame. When nothing is playing this is all zeros, which a
		// Sprite reads as the whole texture, so play something before drawing.
		Rect Frame() const
		{
			return m_data && !m_data->frames.empty() ? m_data->frames[m_index] : Rect{ 0, 0, 0, 0 };
		}

		// True once a non-looping animation has shown its last frame for the full frame time.
		bool Finished() const { return m_done; }

	private:
		const AnimationData*	m_data = nullptr;
		size_t					m_index = 0;
		float					m_time = 0.0f;
		bool					m_loop = true;
		bool					m_done = false;
	};

	// Owns every loaded sheet. Sheets never move once loaded, so the pointers Animators keep
	// into them stay valid for the life of the library.
	class SpriteLibrary
	{
	public:
		// Loads a .cfg and keys it by the file's name without folder or extension:
		// L"Assets\\alucard.cfg" -> "alucard". Returns nullptr when loading fails. A sheet
		// loaded while the library has device resources gets its texture straight away.
		const SpriteSheet* Load(const std::wstring& configFile);

		const SpriteSheet* Find(const std::string& key) const
		{
			const auto it = m_sheets.find(key);
			return it == m_sheets.end() ? nullptr : &it->second;
		}

		// Throws std::out_of_range if the key was never loaded.
		const SpriteSheet& Get(const std::string& key) const { return m_sheets.at(key); }

		// Every sheet's texture, created or let go of along with the device.
		void CreateDeviceDependentResources(SpriteRenderer& sprites);
		void ReleaseDeviceDependentResources();

	private:
		std::map<std::string, SpriteSheet>	m_sheets;
		SpriteRenderer*						m_sprites = nullptr;	// set while device resources exist
	};
}
