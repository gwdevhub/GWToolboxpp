// Desaturate texture RGB, then modulate with vertex diffuse colour.
// Used for mission map icons: texture provides shape/alpha detail as a
// greyscale silhouette; vertex colour provides the affiliation tint.
sampler2D Texture : register(s0);

struct PS_INPUT {
    float4 diffuse : COLOR0;
    float2 uv : TEXCOORD0;
};

float4 main(PS_INPUT input) : COLOR
{
    float4 tex = tex2D(Texture, input.uv);
    float gray = dot(tex.rgb, float3(0.299, 0.587, 0.114));
    return float4(gray * input.diffuse.rgb, tex.a * input.diffuse.a);
}
