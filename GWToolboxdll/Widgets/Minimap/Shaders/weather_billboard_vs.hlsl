// World-space textured billboard: corners are pre-built in world coords on the CPU; this only
// transforms them and passes UV + world position (for the distance fade in the pixel shader).
float4x4 view_matrix : register(c0);
float4x4 proj_matrix : register(c4);

struct VS_INPUT {
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float4 world : TEXCOORD1; // world coords for the distance check
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.color = input.color;
    output.uv = input.uv;
    output.world = float4(input.position, 1.0);
    output.position = mul(float4(input.position, 1.0), view_matrix);
    output.position = mul(output.position, proj_matrix);
    return output;
}
