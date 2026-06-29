// World-space lava/water surface draped on the floor on the CPU. Adds animated waves here: a sum of three
// incommensurate directional sine waves displaces the height, so crests never repeat and come taller/smaller
// at irregular times. The app passes time + a (time-varying) amplitude, so the surface also swells and calms.
float4x4 view_matrix : register(c0);
float4x4 proj_matrix : register(c4);
float4 wave : register(c8); // x = time (s), y = amplitude (world units, already enveloped), z = wave scale

struct VS_INPUT {
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float4 world : TEXCOORD1; // (displaced) world coords for the distance check
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    const float t = wave.x;
    const float s = max(wave.z, 0.0001);
    const float2 p = input.position.xy;

    // Three directional waves with incommensurate frequencies/speeds -> the surface never tiles in time and
    // the interference makes some crests tall, others small, at non-periodic intervals.
    float h = sin(dot(p, float2(0.0130, 0.0090)) * s + t * 1.30);
    h += 0.60 * sin(dot(p, float2(-0.0080, 0.0140)) * s + t * 0.90);
    h += 0.35 * sin(dot(p, float2(0.0170, -0.0060)) * s + t * 1.70);

    float3 wp = input.position;
    wp.z -= h * wave.y; // GW up is -z: subtract to raise crests above the floor

    output.color = input.color;
    output.uv = input.uv;
    output.world = float4(wp, 1.0);
    output.position = mul(float4(wp, 1.0), view_matrix);
    output.position = mul(output.position, proj_matrix);
    return output;
}
