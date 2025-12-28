#include "shared.hlsl"

float4 main(float density : TEXCOORD0, uint hash : TEXCOORD1) : SV_Target {
    float v = saturate((density) / 25);
    
    float3 color = hsv2rgb(float3(frac(hash * 0.6180339887), v, 1));

    return float4(color, 1.0);
}
