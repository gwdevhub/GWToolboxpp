// Two attributes to this shader (provided once per frame):
float4 cur_pos : register(c0);  // only xyz components are used
float4 max_dist : register(c1); // only x component is used
float4 fog_starts_at : register(c2); // only x component is used


// Compute the euclidean distance between two 3D points.
float euclidean(float3 p1, float3 p2) {
	float delta1 = (p1.x - p2.x);
	float delta2 = (p1.y - p2.y);
	float delta3 = (p1.z - p2.z);
	return sqrt(pow(delta1, 2.0) + pow(delta2, 2.0) + pow(delta3, 2.0));
}


// The pixel shader takes, for each pixel, an position, a color value, and a world coordinate.
// All are interoplated by the GPU for the current pixel (fragment).
struct PS_INPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float4 position_for_dist_check : TEXCOORD0;
};


// Main entrypoint for this shader.
float4 main(PS_INPUT input) : COLOR {
    // If this pixel is >= max_dist away, render it fully transparent.
    // `clip(-1)` did not work for unknown reasons.
    float pixel_dist = euclidean(cur_pos.xyz, input.position_for_dist_check.xyz);
    if (pixel_dist >= max_dist.x) {  // completely removed
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    float4 output = input.color;
    if (pixel_dist > fog_starts_at.x) {  // fog is applied
        output.rgba = lerp(output.rgba, float4(0.0, 0.0, 0.0, 0.0), (pixel_dist - fog_starts_at.x) / (max_dist.x - fog_starts_at.x));
    }
    return output;
}
