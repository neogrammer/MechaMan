#include "pch.h"
#include "CidSprite.h"
#include "PackageFiles.h"

using namespace MechaMan;

extern const RectF c_view;


SpriteRenderer* CidSprite::m_spriteRenderer = nullptr;
	

void CidSprite::Create(RECT worldRect, RECT texRect, float speed, float direction)
{
	m_entity.isBadge = true;
	m_entity.size = { (float)(worldRect.right - worldRect.left) };
	m_entity.position = { c_view.width / 2.f, c_view.height / 2.f };

	
	m_entity.velocity = { 0.f,0.f };

	m_entity.angle = DirectX::XM_2PI * 0.4f;
	m_entity.spin = 0.f;

	if (m_entity.isBadge)
	{
		m_entity.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	}
	else
	{
		// A bright color, partly see-through.
		m_entity.color = { 0.3f + 0.7f,  0.3f + 0.1f ,  0.3f + 0.4f, 0.6f };
	}
	
}

void CidSprite::Create(std::shared_ptr<Texture> tex, float speed, float direction)
{
	m_texture = tex;

	auto wd = m_texture->GetWidth();
	auto ht = m_texture->GetHeight();
	RECT rW = { 0, 0, wd, ht };
	RECT rT = { 0, 0, wd, ht };

	Create(rW, rT, speed, direction);
}

CidSprite::CidSprite()
	: m_texture{ nullptr }
{

}

CidSprite::CidSprite(std::shared_ptr<Texture> tex)
	: m_texture{ tex }
{
	auto wd = m_texture->GetWidth();
	auto ht =m_texture->GetHeight();
	RECT rW = { 0, 0, wd, ht };
	RECT rT = { 0, 0, wd, ht };

	Create(rW, rT, 300.f, 0.f);
}

CidSprite::CidSprite(SpriteRenderer* rend, std::shared_ptr<Texture> tex)
	: m_texture{ tex }
{
 CidSprite::m_spriteRenderer = rend;

 auto wd = m_texture->GetWidth();
 auto ht = m_texture->GetHeight();
 RECT rW = { 0, 0, wd, ht };
 RECT rT = { 0, 0, wd, ht };

 Create(rW, rT, 300.f, 0.f);
}

CidSprite::~CidSprite()
{
	CidSprite::m_spriteRenderer = nullptr;
}

CidSprite& CidSprite::operator=(CidSprite&& o)
{
	// TODO: insert return statement here
	this->m_texture.swap(o.m_texture);	
	RECT rW = { 0, 0, 50, 50 };
	RECT rT = { 0, 0, 50, 50 };

	this->Create(rW, rT, 300.f, 0.f);

	return *this;
}

CidSprite& CidSprite::operator=(const CidSprite& o)
{
	// TODO: insert return statement here
	this->m_texture = o.m_texture;
	RECT rW = { 0, 0, 50, 50 };
	RECT rT = { 0, 0, 50, 50 };

	Create(rW, rT, 300.f, 0.f);
	return *this;
}

CidSprite::CidSprite(CidSprite&& o)
{
	m_texture.swap(o.m_texture);

	m_entity = std::move(static_cast<Entity&&>(o.m_entity));

}

CidSprite::CidSprite(const CidSprite& o)
{
	m_texture = o.m_texture;

	m_entity = o.m_entity;

}
