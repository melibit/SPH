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
    Particle p = particleInBuffer[i]; 
    p.pressure = U.gasConstant * (p.density - U.restDensity); 
    particleOutBuffer[i] = p; 
}
