// Procedural soft-edged ring: no texture, no solid band - a triangular profile that ramps 0->1
// approaching `input.radius` (per-vertex, so each ring instance in the batch can differ), peaks
// there, then ramps 1->0 moving past it, smoothstepped for a soft edge on both sides.
float4 cur_pos : register(c0);       // only xyz used: camera focus
float4 max_dist : register(c1);      // only x used
float4 fog_starts_at : register(c2); // only x used
float4 ring_edge : register(c3);     // x = fade width (world units), shared by every ring instance in the draw

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 local : TEXCOORD0;
    float radius : TEXCOORD1;
    float4 world : TEXCOORD2;
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
    float edge = max(ring_edge.x, 0.0001);
    float t = saturate(1.0 - abs(r - input.radius) / edge);
    float ring_alpha = smoothstep(0.0, 1.0, t);

    float4 output = input.color;
    output.a *= ring_alpha;
    if (pixel_dist > fog_starts_at.x) {
        output.a *= 1.0 - saturate((pixel_dist - fog_starts_at.x) / (max_dist.x - fog_starts_at.x));
    }
    return output;
}
