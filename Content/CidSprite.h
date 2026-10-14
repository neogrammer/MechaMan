#pragma once
#include <memory>
#include <utility>
#include "SpriteRenderer.h"
#include "Entity.h"

namespace MechaMan {
	class CidSprite
	{


		// GPU objects. Created and released along with the device.
	public:
		static SpriteRenderer* m_spriteRenderer;

	protected:
		std::shared_ptr<Texture> m_texture;
		Entity m_entity{};
		
		void Create(RECT worldRect, RECT texRect, float speed = 300.f, float direction = 0.f);
	public:

		CidSprite();
		CidSprite(std::shared_ptr<Texture>);
		CidSprite(SpriteRenderer*, std::shared_ptr<Texture>);


		~CidSprite();
		CidSprite& operator=(CidSprite&&);
		CidSprite& operator=(const CidSprite&);
		CidSprite(CidSprite&&);
		CidSprite(const CidSprite&);
		std::shared_ptr<Texture> GetTexture() { return m_texture; }
		void SetTexture(std::shared_ptr<Texture> tex) { m_texture = tex; }
		static  void SetSpriteRenderer(SpriteRenderer* spriteRenderer) { m_spriteRenderer = spriteRenderer; }
		void Create(std::shared_ptr<Texture> tex, float speed = 300.f, float direction = 0.f);
		Entity& GetEntity() { return m_entity; }
	};
}