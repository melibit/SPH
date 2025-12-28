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
  if (i >= U.particleCount) 
    return;

  Particle pi = particleInBuffer[i];
  
  float2 force = float2(0, 0);

  for (uint j = 0; j < U.particleCount; j++) {
    if (i == j) 
      continue;

    Particle pj = particleOutBuffer[j];
    float2 rij = pi.position - pj.position;
    float r = length(rij);

    if (r < U.smoothingRadius) {
      float2 grad = SpikyGrad(rij, U.smoothingRadius);
      force += -U.particleMass * (pi.pressure + pj.pressure) / (2.0 * pj.density) * grad;
    }
  }

  force += U.gravity * pi.density;

  pi.velocity += U.deltaTime * force / pi.density;
  
  particleOutBuffer[i] = pi;
  return;
}
