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
};

struct Particle {
  float2 position;
  float2 velocity;
  float density;
  float pressure;
};

struct HashEntry {
  uint cellHash;
  uint particleIndex;
};

struct CellRange {
  uint start;
  uint count;
};


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
