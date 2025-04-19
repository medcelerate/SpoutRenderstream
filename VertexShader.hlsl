cbuffer ConstantBuffer : register(b0)
{
    float opacity;
};

struct VS_INPUT
{
    float3 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
    float Opacity : TEXCOORD1;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.Pos = float4(input.Pos, 1.0);
    output.Tex = input.Tex;
    output.Opacity = opacity;
    return output;
}
