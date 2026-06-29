// Lava ribbon: samples the bound (tiling) lava texture, scrolls the V coord over time to fake flow,
// boosts brightness for an emissive glow, tints by the vertex colour, and fades alpha with distance
// (same distance scheme as the weather/world-line shaders).
float4 cur_pos : register(c0);       // only xyz used: camera focus
float4 max_dist : register(c1);      // only x used
float4 fog_starts_at : register(c2); // only x used
float4 lava_params : register(c3);   // xy = scroll offset (flow, wandering direction), z = glow strength

sampler2D lava : register(s0);

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
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
    float2 uv = input.uv + lava_params.xy; // scroll in a (slowly wandering) direction
    float4 output = tex2D(lava, uv) * input.color;
    output.rgb *= (1.0 + lava_params.y); // emissive glow boost
    if (pixel_dist > fog_starts_at.x) {
        output.a *= 1.0 - saturate((pixel_dist - fog_starts_at.x) / (max_dist.x - fog_starts_at.x));
    }
    return output;
}
