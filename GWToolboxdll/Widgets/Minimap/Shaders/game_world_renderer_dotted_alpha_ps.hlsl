float4 cur_pos : register(c0);       // only xyz components are used
float4 max_dist : register(c1);      // only x component is used
float4 fog_starts_at : register(c2); // only x component is used
float4 tint : register(c3);          // only alpha component is used
bool use_dotted_effect : register(b0);

float euclidean(float3 p1, float3 p2) {
    float delta1 = (p1.x - p2.x);
    float delta2 = (p1.y - p2.y);
    float delta3 = (p1.z - p2.z);
    return sqrt(delta1 * delta1 + delta2 * delta2 + delta3 * delta3);
}

struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float4 position_for_dist_check : TEXCOORD0;
};

float4 main(PS_INPUT input) : COLOR {
    float pixel_dist = euclidean(cur_pos.xyz, input.position_for_dist_check.xyz);
    if (pixel_dist >= max_dist.x) {  // completely removed
        discard;
    }

    if (use_dotted_effect) {
        float pattern = fmod(pixel_dist, 200.0);
        if (pattern > 100.0) {
            discard;
        }
    }

    float4 output = input.color;
    output.a *= tint.a;
    if (pixel_dist > fog_starts_at.x) {  // fog is applied
        output.rgba = lerp(output.rgba, float4(0.0, 0.0, 0.0, 0.0), (pixel_dist - fog_starts_at.x) / (max_dist.x - fog_starts_at.x));
    }

    return output;
}
