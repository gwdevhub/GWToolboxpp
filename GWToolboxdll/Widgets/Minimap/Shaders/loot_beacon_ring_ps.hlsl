// Procedural soft-edged ring: no texture. Shades a single ring by distance from centre - a triangular
// profile that ramps 0->1 approaching `ring_radius` (shared by every ring instance in this draw call,
// since every beacon pulses on the same shared timer), peaks there, then ramps 1->0 moving past it.
// The fade width is a fixed 4 gwinches, not user-configurable, so it's a compile-time constant here
// rather than another shader register.
static const float RING_EDGE = 4.0; // gwinches

float4 cur_pos : register(c0);       // only xyz used: camera focus
float4 max_dist : register(c1);      // only x used
float4 fog_starts_at : register(c2); // only x used
float4 ring_radius : register(c3);   // only x used: this draw's ring radius, world units

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
    float t = saturate(1.0 - abs(r - ring_radius.x) / RING_EDGE);
    float ring_alpha = smoothstep(0.0, 1.0, t);

    float4 output = input.color;
    output.a *= ring_alpha;
    if (pixel_dist > fog_starts_at.x) {
        output.a *= 1.0 - saturate((pixel_dist - fog_starts_at.x) / (max_dist.x - fog_starts_at.x));
    }
    return output;
}
