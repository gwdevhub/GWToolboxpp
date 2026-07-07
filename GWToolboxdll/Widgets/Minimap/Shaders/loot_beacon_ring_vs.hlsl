// World-space procedural ring quad: corners are pre-built in world coords on the CPU. `local` carries
// each corner's offset from the ring's centre in world units (NOT a 0..1 texture UV); `radius` is this
// ring instance's current target radius (baked per-vertex, not a shared constant, so multiple rings with
// different radii - e.g. two beacons mid-pulse at different phases - can be batched into one draw call).
float4x4 view_matrix : register(c0);
float4x4 proj_matrix : register(c4);

struct VS_INPUT {
    float3 position : POSITION;
    float4 color : COLOR;
    float2 local : TEXCOORD0;
    float radius : TEXCOORD1;
};

struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 local : TEXCOORD0;
    float radius : TEXCOORD1;
    float4 world : TEXCOORD2; // world coords for the distance fade
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.color = input.color;
    output.local = input.local;
    output.radius = input.radius;
    output.world = float4(input.position, 1.0);
    output.position = mul(float4(input.position, 1.0), view_matrix);
    output.position = mul(output.position, proj_matrix);
    return output;
}
