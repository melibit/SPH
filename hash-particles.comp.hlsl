#include "shared.hlsl"

[[vk::binding(0, 0)]]
RWStructuredBuffer<HashEntry> hashEntryOutBuffer : register(u0);

[[vk::binding(1, 0)]]
StructuredBuffer<Particle> particleInBuffer : register(t0);

[[vk::binding(0, 2)]]
ConstantBuffer<Uniforms> U : register(b0);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
  uint i = DTid.x;
  if (i >= U.particleCount)
    return;

  uint hash = HashPosition(particleInBuffer[i].position, U.smoothingRadius, U.hashTableSize);

  hashEntryOutBuffer[i].hash = hash;
  hashEntryOutBuffer[i].particleIndex = i;
}
