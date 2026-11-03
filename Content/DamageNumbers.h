#pragma once

#include <DirectXMath.h>
#include <vector>

namespace MechaMan
{
	class TextRenderer;

	// Numbers that pop up where a hit landed, rise and fade away. They are in world units, so
	// each stays over the actor it belongs to as the camera moves: draw them in the indicator
	// layer's text.
	class DamageNumbers
	{
	public:
		void Spawn(DirectX::XMFLOAT2 at, int amount);
		void Update(float dt);
		void Draw(TextRenderer& text) const;

	private:
		struct Number
		{
			DirectX::XMFLOAT2	at;
			int					amount;
			float				age;	// seconds since it appeared
		};

		std::vector<Number> m_numbers;
	};
}
