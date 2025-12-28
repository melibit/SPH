#include "shared.hlsl"

[[vk::binding(0, 0)]]
StructuredBuffer<Particle> particleBuffer : register(t0);

[[vk::binding(0, 1)]]
ConstantBuffer<Uniforms> U : register(b1);

struct VSOut {
  float4 position : SV_Position;
  float density : TEXCOORD0;
  uint hash : TEXCOORD1;
  [[vk::builtin("PointSize")]]
  float pointSize : PSIZE;
};

VSOut main(uint Vid : SV_VertexID) {
  Particle p = particleBuffer[Vid];
  float2 ndc = p.position * 2.0 - 1.0;
  
  VSOut o;
  o.position = float4(ndc, 0.0, 1.0);
  o.hash = HashPosition(p.position.xy, U.smoothingRadius, U.hashTableSize);
  o.density = p.density;
  o.pointSize = 2.;
  return o;
}
