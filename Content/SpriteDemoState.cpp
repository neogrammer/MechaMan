#include "pch.h"
#include "SpriteDemoState.h"

#include "PackageFiles.h"

#include <cmath>
#include <cstdio>
#include <random>
#include "Entity.h"
using namespace MechaMan;

using namespace DirectX;

namespace
{
	// The part of the game world the screen shows. Positions and sizes are in these units,
	// whatever the size of the window, so the demo looks the same at any resolution.
	const RectF c_view = { 0.0f, 0.0f, 1920.0f, 1080.0f };

	const size_t c_entityCount = 400;

	// A round, shaded ball made in code, for projects without the image the demo looks for.
	std::shared_ptr<Texture> CreateBallTexture(SpriteRenderer& sprites)
	{
		const UINT size = 32;
		std::vector<uint8_t> pixels(size * size * 4);

		for (UINT y = 0; y < size; y++)
		{
			for (UINT x = 0; x < size; x++)
			{
				const float dx = (static_cast<float>(x) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
				const float dy = (static_cast<float>(y) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
				const float distance = sqrtf(dx * dx + dy * dy);

				// Opaque inside the circle, clear outside, lighter towards the top-left.
				const float opacity = distance < 1.0f ? 1.0f : 0.0f;
				const float shade = 1.0f - 0.5f * sqrtf((dx + 0.4f) * (dx + 0.4f) + (dy + 0.4f) * (dy + 0.4f));
				const float brightness = (shade < 0.25f ? 0.25f : shade) * opacity;

				// The color bytes are multiplied by the opacity, as CreateTexture expects.
				uint8_t* pixel = &pixels[(y * size + x) * 4];
				pixel[0] = static_cast<uint8_t>(brightness * 255.0f);
				pixel[1] = static_cast<uint8_t>(brightness * 255.0f);
				pixel[2] = static_cast<uint8_t>(brightness * 255.0f);
				pixel[3] = static_cast<uint8_t>(opacity * 255.0f);
			}
		}

		return sprites.CreateTexture(size, size, pixels.data());
	}
}

SpriteDemoState::SpriteDemoState(GameStateStack& stack, SpriteRenderer& sprites) :
	GameState(stack),
	m_sprites(sprites),
	m_secondsSinceReport(0.0)
{
	// Scatter the entities. A fixed seed gives the same scene every run.
	std::mt19937 random(12345);
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);

	m_entities.resize(c_entityCount);
	for (size_t i = 0; i < c_entityCount; i++)
	{
		Entity& entity = m_entities[i];

		// Every fourth one is a badge, so the two kinds are thoroughly mixed up in the list.
		entity.isBadge = (i % 4 == 0);
		entity.size = entity.isBadge ? 64.0f + 64.0f * unit(random) : 24.0f + 48.0f * unit(random);
		entity.position = { c_view.width * unit(random), c_view.height * unit(random) };

		const float direction = XM_2PI * unit(random);
		const float speed = 60.0f + 240.0f * unit(random);
		entity.velocity = { speed * cosf(direction), speed * sinf(direction) };

		entity.angle = XM_2PI * unit(random);
		entity.spin = (unit(random) - 0.5f) * 4.0f;

		if (entity.isBadge)
		{
			entity.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		}
		else
		{
			// A bright color, partly see-through.
			entity.color = { 0.3f + 0.7f * unit(random), 0.3f + 0.7f * unit(random), 0.3f + 0.7f * unit(random), 0.6f };
		}
	}
}

void SpriteDemoState::CreateDeviceDependentResources(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	// An image from the app package. Projects made from the Visual Studio template have this
	// one; if this project does not, a ball made in code stands in for it.
	if (PackageFiles::Exists(L"Assets\\StoreLogo.png"))
	{
		m_badge = m_sprites.LoadTexture(L"Assets\\StoreLogo.png");
	}
	else
	{
		m_badge = CreateBallTexture(m_sprites);
	}
}

void SpriteDemoState::CreateWindowSizeDependentResources()
{
	// Nothing to do: the view is the same whatever the size of the window.
}

void SpriteDemoState::ReleaseDeviceDependentResources()
{
	m_badge = nullptr;
}

bool SpriteDemoState::IsLoaded() const
{
	return m_badge != nullptr;
}

// Game logic only. Nothing in here touches the GPU.
void SpriteDemoState::Update(DX::StepTimer const& timer)
{
	const float seconds = static_cast<float>(timer.GetElapsedSeconds());

	for (Entity& entity : m_entities)
	{
		entity.position.x += entity.velocity.x * seconds;
		entity.position.y += entity.velocity.y * seconds;
		entity.angle += entity.spin * seconds;

		// Bounce off the edges of the view.
		const float half = entity.size * 0.5f;
		if (entity.position.x < c_view.x + half)
		{
			entity.position.x = c_view.x + half;
			entity.velocity.x = fabsf(entity.velocity.x);
		}
		if (entity.position.x > c_view.x + c_view.width - half)
		{
			entity.position.x = c_view.x + c_view.width - half;
			entity.velocity.x = -fabsf(entity.velocity.x);
		}
		if (entity.position.y < c_view.y + half)
		{
			entity.position.y = c_view.y + half;
			entity.velocity.y = fabsf(entity.velocity.y);
		}
		if (entity.position.y > c_view.y + c_view.height - half)
		{
			entity.position.y = c_view.y + c_view.height - half;
			entity.velocity.y = -fabsf(entity.velocity.y);
		}
	}

	// Once a second, say in the debugger's Output window what the last frame cost.
	m_secondsSinceReport += timer.GetElapsedSeconds();
	if (m_secondsSinceReport >= 1.0)
	{
		m_secondsSinceReport = 0.0;

		wchar_t report[96];
		swprintf_s(report, L"SpriteDemoState: %u sprites drawn with %u draw calls\n", m_sprites.GetSpriteCount(), m_sprites.GetDrawCallCount());
		OutputDebugStringW(report);
	}
}

void SpriteDemoState::Render(ID3D12GraphicsCommandList* commandList)
{
	m_sprites.Begin(c_view);

	// The entities go to the renderer in whatever order the list has them in, badges and
	// squares mixed together. The renderer sorts them out: every square in one draw call,
	// then every badge, on the layer above, in another.
	Sprite sprite;
	sprite.origin = { 0.5f, 0.5f };

	for (const Entity& entity : m_entities)
	{
		sprite.texture = entity.isBadge ? m_badge.get() : nullptr;
		sprite.position = entity.position;
		sprite.size = { entity.size, entity.size };
		sprite.rotation = entity.angle;
		sprite.color = entity.color;
		sprite.layer = entity.isBadge ? 1 : 0;

		m_sprites.Draw(sprite);
	}

	m_sprites.End(commandList);
}
