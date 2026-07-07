// Procedural soft-edged ring: no texture. Shades a single ring by distance from centre - solid
// (alpha=1) within `ring_params.y` (thickness) of `ring_params.x` (radius, shared by every ring
// instance in this draw call, since every beacon pulses on the same shared timer), then a
// smoothstepped fade to transparent over `ring_params.z` (fade distance) beyond that.
float4 cur_pos : register(c0);       // only xyz used: camera focus
float4 max_dist : register(c1);      // only x used
float4 fog_starts_at : register(c2); // only x used
float4 ring_params : register(c3);   // x = radius, y = thickness (solid half-width), z = fade distance - world units

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 local : TEXCOORD0;
    float4 world : TEXCOORD1;
};

float euclidean(float3 a, float3 b) {
    float3 d = a - b;
    return sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

float4 main(PS_INPUT input) : COLOR {
    float pixel_dist = euclidean(cur_pos.xyz, input.world.xyz);
    if (pixel_dist >= max_dist.x) {
        discard;
    }

    float r = length(input.local);
    float fade = max(ring_params.z, 0.0001);
    float past_thickness = max(abs(r - ring_params.x) - ring_params.y, 0.0);
    float t = saturate(1.0 - past_thickness / fade);
    float ring_alpha = smoothstep(0.0, 1.0, t);

    float4 output = input.color;
    output.a *= ring_alpha;
    if (pixel_dist > fog_starts_at.x) {
        output.a *= 1.0 - saturate((pixel_dist - fog_starts_at.x) / (max_dist.x - fog_starts_at.x));
    }
    return output;
}
