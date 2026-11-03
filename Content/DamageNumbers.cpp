#include "pch.h"
#include "DamageNumbers.h"

#include "TextRenderer.h"

#include <algorithm>
#include <string>

using namespace MechaMan;
using namespace DirectX;

namespace
{
	const float c_life = 0.8f;		// seconds on screen
	const float c_rise = 16.0f;		// world units it floats up over its life
	const float c_height = 10.0f;	// how tall the digits are, world units
}

void DamageNumbers::Spawn(XMFLOAT2 at, int amount)
{
	m_numbers.push_back({ at, amount, 0.0f });
}

void DamageNumbers::Update(float dt)
{
	for (Number& number : m_numbers)
	{
		number.age += dt;
	}

	m_numbers.erase(
		std::remove_if(m_numbers.begin(), m_numbers.end(), [](const Number& number) { return number.age >= c_life; }),
		m_numbers.end());
}

void DamageNumbers::Draw(TextRenderer& text) const
{
	const float lineHeight = text.Measure(L"0").y;
	if (lineHeight <= 0.0f)
	{
		return;
	}
	const float scale = c_height / lineHeight;

	for (const Number& number : m_numbers)
	{
		const float t = number.age / c_life;
		const float opacity = t < 0.5f ? 1.0f : 1.0f - (t - 0.5f) * 2.0f;	// solid for half its life, then fades
		const XMFLOAT2 position = { number.at.x, number.at.y - c_rise * t };
		text.Draw(std::to_wstring(number.amount), position, { 1.0f, 0.85f, 0.3f, opacity }, scale, true);
	}
}
