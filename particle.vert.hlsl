#include "shared.hlsl"

[[vk::binding(0, 0)]]
StructuredBuffer<Particle> particleBuffer : register(t0);

[[vk::binding(0, 1)]]
cbuffer Uniform : register(b0) {
  float2 screenSize;
};

struct VSOut {
  float4 position : SV_Position;
  float value : TEXCOORD0;
  [[vk::builtin("PointSize")]]
  float pointSize : PSIZE;
};

VSOut main(uint Vid : SV_VertexID) {
  Particle p = particleBuffer[Vid];

  float2 ndc = p.position * 2.0 - 1.0;
  
  VSOut o;
  o.position = float4(ndc, 0.0, 1.0);
  o.value = p.density;
  o.pointSize = 2.;
  return o;
}
