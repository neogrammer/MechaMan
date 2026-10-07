// Positions a sprite corner on the screen. Corners arrive in the units of the view that
// was passed to SpriteRenderer::Begin, already scaled, rotated and moved into place.
//
// Build setting: in this file's Properties, under HLSL Compiler > General, Shader Type has
// to be "Vertex Shader (/vs)".
cbuffer SpriteConstantBuffer : register(b0)
{
	matrix viewProjection;
};

struct VertexShaderInput
{
	float2 pos : POSITION;
	float2 tex : TEXCOORD0;
	float4 color : COLOR0;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
	float4 color : COLOR0;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	output.pos = mul(float4(input.pos, 0.0f, 1.0f), viewProjection);
	output.tex = input.tex;
	output.color = input.color;

	return output;
}
