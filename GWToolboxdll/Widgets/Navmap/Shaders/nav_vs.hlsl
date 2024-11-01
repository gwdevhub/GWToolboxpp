column_major float4x4 wvp : register(c0);

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.color = input.color;
	output.position = mul(float4(input.position, 1.0), wvp);
    return output;
}
