// Textured weather sprite: samples the bound texture, tints by the vertex colour, and fades the
// alpha out with distance (same scheme as the world line shader, but on alpha so sprites dissolve).
float4 cur_pos : register(c0);       // only xyz used: camera focus
float4 max_dist : register(c1);      // only x used
float4 fog_starts_at : register(c2); // only x used

sampler2D sprite : register(s0);

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
    float4 output = tex2D(sprite, input.uv) * input.color;
    if (pixel_dist > fog_starts_at.x) {
        output.a *= 1.0 - saturate((pixel_dist - fog_starts_at.x) / (max_dist.x - fog_starts_at.x));
    }
    return output;
}
