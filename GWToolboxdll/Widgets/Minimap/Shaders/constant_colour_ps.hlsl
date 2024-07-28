cbuffer cbPerFrame : register(c0)
{
    float4 color : register(c0);
}

float4 main() : SV_TARGET
{
    return color;
}
