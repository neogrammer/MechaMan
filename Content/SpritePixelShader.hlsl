// Colors one pixel of a sprite: the texture at that point, multiplied by the sprite's tint.
// Both are premultiplied by their opacity, and so is the result.
//
// Build setting: in this file's Properties, under HLSL Compiler > General, Shader Type has
// to be "Pixel Shader (/ps)".
Texture2D spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
	float4 color : COLOR0;
};

float4 main(PixelShaderInput input) : SV_Target
{
	return spriteTexture.Sample(spriteSampler, input.tex) * input.color;
}
