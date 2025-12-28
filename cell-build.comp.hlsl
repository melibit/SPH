#include "shared.hlsl"

[[vk::binding(0, 0)]]
RWStructuredBuffer<CellRange> cellRangeOutBuffer : register(u0);

[[vk::binding(1, 0)]]
StructuredBuffer<HashEntry> hashEntryInBuffer : register(t1);

[[vk::binding(0, 2)]]
ConstantBuffer<Uniforms> U : register(b0);

[numthreads(64,1,1)]
void main(uint id : SV_DispatchThreadID) {
  if (id >= U.particleCount) 
    return;

  uint hash = hashEntryInBuffer[id].hash;

  if (id == 0 || hash != hashEntryInBuffer[id - 1].hash) {
    cellRangeOutBuffer[hash].start = id;
    cellRangeOutBuffer[hash].count = 1;
  } 
  else {
    InterlockedAdd(cellRangeOutBuffer[hash].count, 1);
  }
}
