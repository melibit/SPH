#include <SDL3/SDL.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_PARTICLES 16384
#define HASHTABLE_SIZE 65537
#define TIME_CONSTANT 5e-5
#define DT_MAX 50

typedef uint32_t uint;

typedef struct float2 {
  float x, y;
} float2;

typedef struct Uniforms {
  float time;
  float deltaTime;
  float smoothingRadius;
  float particleMass;
  float restDensity;
  float gasConstant;
  float2 gravity;
  uint particleCount;
  uint hashTableSize;
} Uniforms;

typedef struct Particle {
  float2 position;
  float2 velocity;
  float density;
  float pressure;
} Particle;

typedef struct HashEntry {
  uint hash;
  uint particleIndex;
} HashEntry;

typedef struct CellRange {
  uint start;
  uint count;
} CellRange;

typedef struct Context {
  SDL_Window *Window;
  SDL_GPUDevice *Device;

  SDL_GPUGraphicsPipeline *ParticleRenderPipeline;

  SDL_GPUComputePipeline *DensityPipeline;
  SDL_GPUComputePipeline *PressurePipeline;
  SDL_GPUComputePipeline *ForcesPipeline;
  SDL_GPUComputePipeline *IntegratePipeline;

  SDL_GPUComputePipeline *CellClearPipeline;
  SDL_GPUComputePipeline *HashParticlePipeline;
  SDL_GPUComputePipeline *HashSortPipeline;
  SDL_GPUComputePipeline *CellBuildPipeline;

  SDL_GPUBuffer *ParticleBufferRead;
  SDL_GPUBuffer *ParticleBufferWrite;
  SDL_GPUBuffer *CellRangeBuffer;
  SDL_GPUBuffer *HashEntryBuffer;

  SDL_GPUTransferBuffer *ParticleUploadBuffer;

  Particle particles[NUM_PARTICLES];

  Uniforms UniformValues;

  float deltaTime;
  bool quit;
} Context;

typedef struct ShaderInfo {
  void *code;
  size_t codeSize;
  SDL_GPUShaderFormat format;
  const char *entrypoint;
} ShaderInfo;

ShaderInfo LoadShaderCode(Context *context, char *shaderName) {
  char shaderPath[256];
  SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(context->Device);
  SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
  const char *entrypoint;

  if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
    snprintf(shaderPath, sizeof(shaderPath), "%s/%s.spv", SDL_GetBasePath(),
             shaderName);
    format = SDL_GPU_SHADERFORMAT_SPIRV;
    entrypoint = "main";
  } else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
    snprintf(shaderPath, sizeof(shaderPath), "%s/%s.msl", SDL_GetBasePath(),
             shaderName);
    format = SDL_GPU_SHADERFORMAT_MSL;
    entrypoint = "main0";
  } else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) {
    snprintf(shaderPath, sizeof(shaderPath), "%s/%s.dxil", SDL_GetBasePath(),
             shaderName);
    format = SDL_GPU_SHADERFORMAT_DXIL;
    entrypoint = "main";
  } else {
    SDL_Log("Invalid SDL_GPUShaderFormat");
    exit(EXIT_FAILURE);
  }

  size_t codeSize;
  void *code = SDL_LoadFile(shaderPath, &codeSize);
  return (ShaderInfo){.code = code,
                      .codeSize = codeSize,
                      .entrypoint = entrypoint,
                      .format = format};
}

SDL_GPUComputePipeline *
InitComputePipeline(Context *context, char *shaderName,
                    SDL_GPUComputePipelineCreateInfo createInfo) {
  ShaderInfo shaderInfo = LoadShaderCode(context, shaderName);
  createInfo.code = shaderInfo.code;
  createInfo.code_size = shaderInfo.codeSize;
  createInfo.entrypoint = shaderInfo.entrypoint;
  createInfo.format = shaderInfo.format;

  SDL_GPUComputePipeline *pipeline =
      SDL_CreateGPUComputePipeline(context->Device, &createInfo);

  SDL_free(shaderInfo.code);
  return pipeline;
}

SDL_GPUShader *InitShader(Context *context, char *shaderName,
                          SDL_GPUShaderCreateInfo createInfo) {
  ShaderInfo shaderInfo = LoadShaderCode(context, shaderName);
  createInfo.code = shaderInfo.code;
  createInfo.code_size = shaderInfo.codeSize;
  createInfo.entrypoint = shaderInfo.entrypoint;
  createInfo.format = shaderInfo.format;
  SDL_GPUShader *shader = SDL_CreateGPUShader(context->Device, &createInfo);
  SDL_free(shaderInfo.code);
  return shader;
}

SDL_GPUGraphicsPipeline *
InitGrahpicsPipeline(Context *context, char *vertName, char *fragName,
                     SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo,
                     SDL_GPUShaderCreateInfo vertCreateInfo,
                     SDL_GPUShaderCreateInfo fragCreateInfo) {

  pipelineCreateInfo.vertex_shader =
      InitShader(context, vertName, vertCreateInfo);
  pipelineCreateInfo.fragment_shader =
      InitShader(context, fragName, fragCreateInfo);
  return SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
}

void Init(Context *context) {
  SDL_Init(SDL_INIT_VIDEO);

  context->Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV |
                                            SDL_GPU_SHADERFORMAT_MSL |
                                            SDL_GPU_SHADERFORMAT_DXIL,
                                        true, NULL);

  context->Window = SDL_CreateWindow(
      "GPU", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

  SDL_ClaimWindowForGPUDevice(context->Device, context->Window);

  SDL_GPUComputePipelineCreateInfo pipelineCreateInfo = {
      .num_readwrite_storage_buffers = 1,
      .num_readonly_storage_buffers = 1,
      .num_uniform_buffers = 1,
      .threadcount_x = 64,
      .threadcount_y = 1,
      .threadcount_z = 1,
  };
  context->DensityPipeline =
      InitComputePipeline(context, "density.comp", pipelineCreateInfo);
  context->PressurePipeline =
      InitComputePipeline(context, "pressure.comp", pipelineCreateInfo);
  context->ForcesPipeline =
      InitComputePipeline(context, "forces.comp", pipelineCreateInfo);
  context->IntegratePipeline =
      InitComputePipeline(context, "integrate.comp", pipelineCreateInfo);

  context->HashParticlePipeline =
      InitComputePipeline(context, "hash-particles.comp", pipelineCreateInfo);

  context->CellBuildPipeline =
      InitComputePipeline(context, "cell-build.comp", pipelineCreateInfo);

  pipelineCreateInfo.num_readonly_storage_buffers = 0;

  context->CellClearPipeline =
      InitComputePipeline(context, "cell-clear.comp", pipelineCreateInfo);

  pipelineCreateInfo.threadcount_x = 1;

  context->HashSortPipeline =
      InitComputePipeline(context, "hash-sort.comp", pipelineCreateInfo);

  SDL_GPUBufferCreateInfo particleBufferCreateInfo = {
      .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
               SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
      .size = sizeof(Particle) * NUM_PARTICLES,
  };

  context->ParticleBufferRead =
      SDL_CreateGPUBuffer(context->Device, &particleBufferCreateInfo);

  context->ParticleBufferWrite =
      SDL_CreateGPUBuffer(context->Device, &particleBufferCreateInfo);

  SDL_GPUBufferCreateInfo cellBufferCreateInfo = {
      .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
               SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
      .size = sizeof(CellRange) * HASHTABLE_SIZE,
  };

  context->CellRangeBuffer =
      SDL_CreateGPUBuffer(context->Device, &cellBufferCreateInfo);

  SDL_GPUBufferCreateInfo hashBufferCreateInfo = {
      .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
               SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
      .size = sizeof(CellRange) * HASHTABLE_SIZE,
  };

  context->HashEntryBuffer =
      SDL_CreateGPUBuffer(context->Device, &hashBufferCreateInfo);

  SDL_GPUTransferBufferCreateInfo particleUploadCreateInfo = {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = sizeof(context->particles),
  };

  context->ParticleUploadBuffer =
      SDL_CreateGPUTransferBuffer(context->Device, &particleUploadCreateInfo);

  SDL_GPUVertexInputState vertexInputState = {
      .num_vertex_buffers = 0,
      .num_vertex_attributes = 0,
  };

  SDL_GPUGraphicsPipelineCreateInfo renderCreateInfo = {
      .vertex_input_state = vertexInputState,
      .primitive_type = SDL_GPU_PRIMITIVETYPE_POINTLIST,
      .rasterizer_state =
          {
              .fill_mode = SDL_GPU_FILLMODE_FILL,
              .cull_mode = SDL_GPU_CULLMODE_NONE,
              .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
          },
      .multisample_state =
          {
              .sample_count = SDL_GPU_SAMPLECOUNT_1,
          },
      .depth_stencil_state =
          {
              .enable_depth_test = false,
              .enable_depth_write = false,
          },
      .target_info =
          {
              .num_color_targets = 1,
              .color_target_descriptions =
                  (SDL_GPUColorTargetDescription[]){
                      {
                          .format = SDL_GetGPUSwapchainTextureFormat(
                              context->Device, context->Window),
                          .blend_state =
                              {
                                  .enable_blend = false,
                              },
                      },
                  },

          },
      .props = 0,
  };

  SDL_GPUShaderCreateInfo vertCreateInfo = {
      .num_storage_buffers = 1,
      .num_uniform_buffers = 1,
      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
  };

  SDL_GPUShaderCreateInfo fragCreateInfo = {
      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
  };

  context->ParticleRenderPipeline =
      InitGrahpicsPipeline(context, "particle.vert", "particle.frag",
                           renderCreateInfo, vertCreateInfo, fragCreateInfo);

  float spacing = sqrtf(1. / (float)NUM_PARTICLES);
  context->UniformValues.smoothingRadius = spacing * 10;
  context->UniformValues.restDensity = 1.0f;
  context->UniformValues.particleMass = spacing * spacing;
  context->UniformValues.gasConstant = 10.0f;
  context->UniformValues.gravity = (float2){0.0f, -9.8f};
  context->UniformValues.particleCount = NUM_PARTICLES;
  context->UniformValues.hashTableSize = HASHTABLE_SIZE;
}

void Update(Context *context) {
  context->UniformValues.deltaTime =
      SDL_min(context->deltaTime * TIME_CONSTANT, DT_MAX * TIME_CONSTANT);
  context->UniformValues.time += context->UniformValues.deltaTime;

  char buf[256];
  snprintf(buf, sizeof(buf), "deltaTime: %.4fms | %.0fFPS", context->deltaTime,
           1000.0f / context->deltaTime);
  SDL_SetWindowTitle(context->Window, buf);
}

void UploadParticles(Context *context) {
  context->UniformValues.particleCount = NUM_PARTICLES;

  void *particleUploadMap = SDL_MapGPUTransferBuffer(
      context->Device, context->ParticleUploadBuffer, false);
  SDL_memcpy(particleUploadMap, context->particles, sizeof(context->particles));
  SDL_UnmapGPUTransferBuffer(context->Device, context->ParticleUploadBuffer);

  SDL_GPUCommandBuffer *commandBuffer =
      SDL_AcquireGPUCommandBuffer(context->Device);

  SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);
  SDL_UploadToGPUBuffer(
      copyPass,
      &(SDL_GPUTransferBufferLocation){
          .transfer_buffer = context->ParticleUploadBuffer, .offset = 0},
      &(SDL_GPUBufferRegion){.buffer = context->ParticleBufferRead,
                             .offset = 0,
                             .size = sizeof(context->particles)},
      true);
  SDL_UploadToGPUBuffer(
      copyPass,
      &(SDL_GPUTransferBufferLocation){
          .transfer_buffer = context->ParticleUploadBuffer, .offset = 0},
      &(SDL_GPUBufferRegion){.buffer = context->ParticleBufferWrite,
                             .offset = 0,
                             .size = sizeof(context->particles)},
      true);
  SDL_EndGPUCopyPass(copyPass);
  SDL_SubmitGPUCommandBuffer(commandBuffer);
}

void SwapParticleBuffers(Context *context) {
  SDL_GPUBuffer *tmp = context->ParticleBufferRead;
  context->ParticleBufferRead = context->ParticleBufferWrite;
  context->ParticleBufferWrite = tmp;
}

void ComputeSpatialGrid(SDL_GPUCommandBuffer *commandBuffer, Context *context) {
  SDL_GPUStorageBufferReadWriteBinding cellWriteBinding = {
      .buffer = context->CellRangeBuffer,
      .cycle = false,
  };
  SDL_GPUStorageBufferReadWriteBinding hashWriteBinding = {
      .buffer = context->HashEntryBuffer,
      .cycle = false,
  };
  {
    SDL_GPUComputePass *clearPass =
        SDL_BeginGPUComputePass(commandBuffer, NULL, 0, &cellWriteBinding, 1);
    SDL_BindGPUComputePipeline(clearPass, context->CellClearPipeline);
    SDL_PushGPUComputeUniformData(commandBuffer, 0, &context->UniformValues,
                                  sizeof(Uniforms));
    SDL_DispatchGPUCompute(clearPass, (HASHTABLE_SIZE + 63) / 64, 1, 1);
    SDL_EndGPUComputePass(clearPass);
  }
  {
    SDL_GPUComputePass *hashPass =
        SDL_BeginGPUComputePass(commandBuffer, NULL, 0, &hashWriteBinding, 1);
    SDL_BindGPUComputePipeline(hashPass, context->HashParticlePipeline);
    SDL_BindGPUComputeStorageBuffers(hashPass, 0, &context->ParticleBufferRead,
                                     1);
    SDL_PushGPUComputeUniformData(commandBuffer, 0, &context->UniformValues,
                                  sizeof(Uniforms));
    SDL_DispatchGPUCompute(hashPass, (NUM_PARTICLES + 63) / 64, 1, 1);
    SDL_EndGPUComputePass(hashPass);
  }
  {
    SDL_GPUComputePass *sortPass =
        SDL_BeginGPUComputePass(commandBuffer, NULL, 0, &hashWriteBinding, 1);
    SDL_BindGPUComputePipeline(sortPass, context->HashSortPipeline);
    SDL_PushGPUComputeUniformData(commandBuffer, 0, &context->UniformValues,
                                  sizeof(Uniforms));
    SDL_DispatchGPUCompute(sortPass, 1, 1, 1);
    SDL_EndGPUComputePass(sortPass);
  }
  {
    SDL_GPUComputePass *buildPass =
        SDL_BeginGPUComputePass(commandBuffer, NULL, 0, &cellWriteBinding, 1);
    SDL_BindGPUComputePipeline(buildPass, context->CellBuildPipeline);
    SDL_BindGPUComputeStorageBuffers(buildPass, 0, &context->HashEntryBuffer,
                                     1);
    SDL_PushGPUComputeUniformData(commandBuffer, 0, &context->UniformValues,
                                  sizeof(Uniforms));
    SDL_DispatchGPUCompute(buildPass, (NUM_PARTICLES + 63) / 64, 1, 1);
    SDL_EndGPUComputePass(buildPass);
  }
}
void ComputeSPH(SDL_GPUCommandBuffer *commandBuffer, Context *context) {
  SDL_GPUStorageBufferReadWriteBinding particleWriteBinding = {
      .buffer = context->ParticleBufferWrite,
      .cycle = false,
  };

  SwapParticleBuffers(context);
  {
    SDL_GPUComputePass *densityComputePass = SDL_BeginGPUComputePass(
        commandBuffer, NULL, 0, &particleWriteBinding, 1);
    SDL_BindGPUComputePipeline(densityComputePass, context->DensityPipeline);
    SDL_BindGPUComputeStorageBuffers(densityComputePass, 0,
                                     &context->ParticleBufferRead, 1);
    SDL_PushGPUComputeUniformData(commandBuffer, 0, &context->UniformValues,
                                  sizeof(Uniforms));
    SDL_DispatchGPUCompute(densityComputePass, (NUM_PARTICLES + 63) / 64, 1, 1);
    SDL_EndGPUComputePass(densityComputePass);
  }
  {
    SDL_GPUComputePass *pressureComputePass = SDL_BeginGPUComputePass(
        commandBuffer, NULL, 0, &particleWriteBinding, 1);
    SDL_BindGPUComputePipeline(pressureComputePass, context->PressurePipeline);
    SDL_BindGPUComputeStorageBuffers(pressureComputePass, 0,
                                     &context->ParticleBufferRead, 1);
    SDL_PushGPUComputeUniformData(commandBuffer, 0, &context->UniformValues,
                                  sizeof(Uniforms));
    SDL_DispatchGPUCompute(pressureComputePass, (NUM_PARTICLES + 63) / 64, 1,
                           1);
    SDL_EndGPUComputePass(pressureComputePass);
  }
  {
    SDL_GPUComputePass *forcesComputePass = SDL_BeginGPUComputePass(
        commandBuffer, NULL, 0, &particleWriteBinding, 1);
    SDL_BindGPUComputePipeline(forcesComputePass, context->ForcesPipeline);
    SDL_BindGPUComputeStorageBuffers(forcesComputePass, 0,
                                     &context->ParticleBufferRead, 1);
    SDL_PushGPUComputeUniformData(commandBuffer, 0, &context->UniformValues,
                                  sizeof(Uniforms));
    SDL_DispatchGPUCompute(forcesComputePass, (NUM_PARTICLES + 63) / 64, 1, 1);
    SDL_EndGPUComputePass(forcesComputePass);
  }
  SwapParticleBuffers(context);
  {
    SDL_GPUComputePass *integrateComputePass = SDL_BeginGPUComputePass(
        commandBuffer, NULL, 0, &particleWriteBinding, 1);
    SDL_BindGPUComputePipeline(integrateComputePass,
                               context->IntegratePipeline);
    SDL_BindGPUComputeStorageBuffers(integrateComputePass, 0,
                                     &context->ParticleBufferRead, 1);
    SDL_PushGPUComputeUniformData(commandBuffer, 0, &context->UniformValues,
                                  sizeof(Uniforms));
    SDL_DispatchGPUCompute(integrateComputePass, (NUM_PARTICLES + 63) / 64, 1,
                           1);
    SDL_EndGPUComputePass(integrateComputePass);
  }
  SwapParticleBuffers(context);
}

void Render(SDL_GPUCommandBuffer *commandBuffer, Context *context) {
  SDL_GPUTexture *swapchainTexture = NULL;
  if (!SDL_AcquireGPUSwapchainTexture(commandBuffer, context->Window,
                                      &swapchainTexture, NULL, NULL))
    return;

  if (swapchainTexture == NULL)
    return;

  SDL_GPUColorTargetInfo colorTarget = {
      .texture = swapchainTexture,
      .load_op = SDL_GPU_LOADOP_CLEAR,
      .store_op = SDL_GPU_STOREOP_STORE,
      .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
  };

  SDL_GPURenderPass *renderPass =
      SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, NULL);

  SDL_BindGPUGraphicsPipeline(renderPass, context->ParticleRenderPipeline);
  SDL_BindGPUVertexStorageBuffers(renderPass, 0, &context->ParticleBufferRead,
                                  1);
  int w, h;
  SDL_GetWindowSize(context->Window, &w, &h);
  float2 screenSize = {
      (float)w,
      (float)h,
  };
  SDL_PushGPUVertexUniformData(commandBuffer, 1, &screenSize, sizeof(float2));

  SDL_DrawGPUPrimitives(renderPass, NUM_PARTICLES, 1, 0, 0);

  SDL_EndGPURenderPass(renderPass);
}

void Bundle(Context *context) {
  SDL_GPUCommandBuffer *commandBuffer =
      SDL_AcquireGPUCommandBuffer(context->Device);

  ComputeSpatialGrid(commandBuffer, context);

  ComputeSPH(commandBuffer, context);

  Render(commandBuffer, context);

  SDL_SubmitGPUCommandBuffer(commandBuffer);

  SDL_WaitForGPUIdle(context->Device);
}

void Exit(Context *context) {
  SDL_ReleaseGPUBuffer(context->Device, context->ParticleBufferRead);
  SDL_ReleaseGPUBuffer(context->Device, context->ParticleBufferWrite);
  SDL_ReleaseGPUComputePipeline(context->Device, context->ForcesPipeline);
  SDL_ReleaseGPUComputePipeline(context->Device, context->IntegratePipeline);
  SDL_ReleaseGPUComputePipeline(context->Device, context->PressurePipeline);
  SDL_ReleaseGPUComputePipeline(context->Device, context->DensityPipeline);
  SDL_ReleaseWindowFromGPUDevice(context->Device, context->Window);
  SDL_DestroyGPUDevice(context->Device);
  SDL_DestroyWindow(context->Window);
}

int main(int argc, char **argv) {
  Context context = {0};
  srand(time(NULL));

  Init(&context);

  for (size_t i = 0; i < sizeof(context.particles) / sizeof(Particle); i++) {
    context.particles[i].position.x = ((float)rand() / RAND_MAX) / 16 + 0.5;
    context.particles[i].position.y = ((float)rand() / RAND_MAX) / 2 + 1.5;
    context.particles[i].velocity.y = -(float)(rand()) / RAND_MAX;
  }

  UploadParticles(&context);

  uint64_t lastTime, newTime;
  lastTime = SDL_GetPerformanceCounter();
  while (!context.quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_EVENT_QUIT:
        context.quit = true;
        break;
      }
    }

    newTime = SDL_GetPerformanceCounter();
    context.deltaTime =
        (newTime - lastTime) * 1000.0f / (float)SDL_GetPerformanceFrequency();
    lastTime = newTime;

    Update(&context);

    Bundle(&context);
  }
  Exit(&context);
  return EXIT_SUCCESS;
}
