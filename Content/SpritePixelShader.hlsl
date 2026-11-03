// Colors one pixel of a sprite: the texture at that point, multiplied by the sprite's tint.
// Both are premultiplied by their opacity, and so is the result.
//
// Build setting: in this file's Properties, under HLSL Compiler > General, Shader Type has
// to be "Pixel Shader (/ps)".
Texture2D spriteTexture : register(t0);
SamplerState crispSampler : register(s0);	// point sampling: pixel art stays sharp
SamplerState smoothSampler : register(s1);	// linear sampling: smoothed (Sprite::smooth)

cbuffer SpriteFilter : register(b1)
{
	uint smooth;	// set for each draw call by SpriteRenderer::End
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
	float4 color : COLOR0;
};

float4 main(PixelShaderInput input) : SV_Target
{
	float4 texel = smooth != 0
		? spriteTexture.Sample(smoothSampler, input.tex)
		: spriteTexture.Sample(crispSampler, input.tex);

	return texel * input.color;
}
