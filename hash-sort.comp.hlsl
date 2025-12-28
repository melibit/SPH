#include "shared.hlsl"

[[vk::binding(0, 0)]]
RWStructuredBuffer<HashEntry> hashEntryBuffer : register(u0);

[[vk::binding(0, 2)]]
ConstantBuffer<Uniforms> U : register(b0);

[numthreads(1,1,1)]
void main(uint id : SV_DispatchThreadID) {
    for (uint i = 1; i < U.particleCount; i++) {
        HashEntry key = hashEntryBuffer[i];
        int j = int(i) - 1;

        while (j >= 0 && hashEntryBuffer[j].hash > key.hash) {
            hashEntryBuffer[j + 1] = hashEntryBuffer[j];
            j--;
        }

        hashEntryBuffer[j + 1] = key;
    }
}
