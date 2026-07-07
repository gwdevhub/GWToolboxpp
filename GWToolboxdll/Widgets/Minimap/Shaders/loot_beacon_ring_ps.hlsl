// Procedural soft-edged ring: no texture. Shades a single ring band by distance from centre -
// a triangular profile that ramps 0->1 approaching `ring_radius`, then 1->0 moving past it, so the
// whole band is `2 * ring_edge` wide with a soft (smoothstepped) fade on both the inner and outer edge.
float4 cur_pos : register(c0);       // only xyz used: camera focus
float4 max_dist : register(c1);      // only x used
float4 fog_starts_at : register(c2); // only x used
float4 ring_params : register(c3);   // x = ring_radius (world units), y = ring_edge (fade width, world units)

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
    float edge = max(ring_params.y, 0.0001);
    float t = saturate(1.0 - abs(r - ring_params.x) / edge);
    float ring_alpha = smoothstep(0.0, 1.0, t);

    float4 output = input.color;
    output.a *= ring_alpha;
    if (pixel_dist > fog_starts_at.x) {
        output.a *= 1.0 - saturate((pixel_dist - fog_starts_at.x) / (max_dist.x - fog_starts_at.x));
    }
    return output;
}
