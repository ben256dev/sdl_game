#pragma once

#include <SDL3/SDL_gpu.h>
#include <stdbool.h>

bool gpu_load_texture_rgba8(SDL_GPUDevice *device, const char *path, SDL_GPUTexture **out_tex, Uint32 *out_w, Uint32 *out_h);
SDL_GPUSampler *gpu_create_sampler_repeat_linear(SDL_GPUDevice *device);

