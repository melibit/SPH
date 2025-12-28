#include "shared.hlsl"

[[vk::binding(0, 0)]]
RWStructuredBuffer<CellRange> cellRangesOutBuffer : register(u0);

[[vk::binding(0, 2)]]
ConstantBuffer<Uniforms> U : register(b0);

[numthreads(64, 1, 1)]
void main(uint DTid : SV_DispatchThreadID) {
  uint i = DTid.x;
  if (i >= U.hashTableSize)
    return;
  cellRangesOutBuffer[i].start = 0;
  cellRangesOutBuffer[i].count = 0;
}
