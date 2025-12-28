float4 main(float value : TEXCOORD0) : SV_Target {
    float v = saturate((value) / 25);
    
    float3 color = lerp(
        float3(0.0, 0.0, 1.0),
        float3(1.0, 0.0, 0.0),
        v
    );

    return float4(color, 1.0);
}
