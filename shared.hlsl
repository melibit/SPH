struct Uniforms {
  float time;
  float deltaTime;
  float smoothingRadius;
  float particleMass;
  float restDensity;
  float gasConstant;
  float2 gravity;
  uint particleCount;
  uint hashTableSize;
  float2 screenSize;
};

struct Particle {
  float2 position;
  float2 velocity;
  float density;
  float pressure;
};

struct HashEntry {
  uint hash;
  uint particleIndex;
};

struct CellRange {
  uint start;
  uint count;
};

uint HashCell(uint2 cell) {
  const uint p1 = 73856093;
  const uint p2 = 19349663;
  return (uint(cell.x) * p1) ^ (uint(cell.y) * p2);
}

uint HashPosition(float2 position, float r, uint s) {
  return HashCell(int2(floor(position / r))) % s;
}

float Poly6(float r, float h) {
  if (r >= 0 && r <= h) {
    float x = (h * h - r * r);
    return (315.0 / (64.0 * 3.141592 * pow(h, 9))) * x * x * x;
  }
  return 0;
}

float2 SpikyGrad(float2 r, float h) {
  float len = length(r);
  if (len > 0 && len <= h) {
    float coeff = -45.0 / (3.141592 * pow(h, 6));
    return coeff * pow(h - len, 2) * (r / len);
  }
  return float2(0, 0);
}

float3 hsv2rgb(float3 c) {
    float4 K = float4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}
