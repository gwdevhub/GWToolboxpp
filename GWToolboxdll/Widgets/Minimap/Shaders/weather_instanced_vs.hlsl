// Instanced world-space billboard. Stream 0 is a static unit quad (corner sign in xy, uv in zw);
// stream 1 is one per-drop record (world centre + the two half-extent axes). The quad is expanded on
// the GPU, so the CPU uploads ~36 bytes per drop instead of building four textured vertices. Output
// matches weather_billboard_vs so the same pixel shader (distance fade) is reused.
float4x4 view_matrix : register(c0);
float4x4 proj_matrix : register(c4);
float4 tint : register(c8);  // condition tint (rgba), constant per draw - all of a draw's particles share it
float4 flags : register(c9); // x = flip texture V (rain streak runs along the downward velocity)

struct VS_INPUT {
    float2 corner : POSITION;   // stream 0: -1/+1 per corner
    float2 uv : TEXCOORD0;      // stream 0
    float3 center : TEXCOORD1;  // stream 1 (per instance)
    float3 axis_x : TEXCOORD2;  // stream 1
    float3 axis_y : TEXCOORD3;  // stream 1
    float alpha : TEXCOORD4;    // stream 1: per-instance alpha multiplier (1 for flakes, fade for settle)
};

struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float4 world : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input) {
    float3 world = input.center + input.corner.x * input.axis_x + input.corner.y * input.axis_y;
    VS_OUTPUT output;
    output.color = float4(tint.rgb, tint.a * input.alpha);
    output.uv = float2(input.uv.x, lerp(input.uv.y, 1.0 - input.uv.y, flags.x));
    output.world = float4(world, 1.0);
    output.position = mul(float4(world, 1.0), view_matrix);
    output.position = mul(output.position, proj_matrix);
    return output;
}
