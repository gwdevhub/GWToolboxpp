// Note: World matrix is absent - we're dealing with world coords as inputs.
float4x4 view_matrix : register(c0);
float4x4 proj_matrix : register(c4);


// The vertex shader takes, for each vertex, a position and color value.
struct VS_INPUT {
    float3 position : POSITION;
    float4 color : COLOR;
};


// The vertex shader produces, for each vertex, a position, a color value, and a distance check position value.
// The distance check value is in world coords, the `position` (SV_POSITION) is always in clip space.
struct VS_OUTPUT {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float4 position_for_dist_check : TEXCOORD0;
};


// Main entrypoint for this shader.
VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    // Pass the vertex color through to the pixel shader untouched.
    output.color = input.color;
    
    // Provide the pixel shader the world coords (input coords) for the vertex.
    output.position_for_dist_check = float4(input.position, 1.0);
    
    // SV_POSITION is then multiplied with the View and Projection matrices.
    output.position = float4(input.position, 1.0);
    output.position = mul(output.position, view_matrix);
    output.position = mul(output.position, proj_matrix);
    return output;
}
