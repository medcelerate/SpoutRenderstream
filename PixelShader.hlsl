
Texture2D gMyTexture : register(t0);
// slot s0
SamplerState gMySampler : register(s0);

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

float4 main(PS_INPUT IN) : SV_Target
{
    // just forward the sampled color
    return gMyTexture.Sample(gMySampler, IN.UV);
   // return gInputTex.Sample(gSampler, IN.UV);
}