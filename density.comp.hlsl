#include "shared.hlsl"

[[vk::binding(0, 0)]]
RWStructuredBuffer<Particle> particleOutBuffer : register(u0);

[[vk::binding(1, 0)]]
StructuredBuffer<Particle> particleInBuffer : register(t0);

[[vk::binding(0, 2)]]
ConstantBuffer<Uniforms> U : register(b0);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
  uint i = DTid.x;
  if (i >= U.particleCount) return;

  Particle p = particleInBuffer[i];
  float density = 0.0;

  for (uint j = 0; j < U.particleCount; j++) {
    float2 rij = p.position - particleInBuffer[j].position;
    float r = length(rij);
    density += U.particleMass * Poly6(r, U.smoothingRadius);
  }

  p.density = max(density, U.restDensity * 0.1);
  particleOutBuffer[i] = p;
}
