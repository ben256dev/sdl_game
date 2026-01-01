#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static void *read_entire_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return NULL;
    }
    void *buf = malloc((size_t)sz);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz)
    {
        free(buf);
        return NULL;
    }
    *out_size = (size_t)sz;
    return buf;
}

static SDL_GPUShader *load_spirv_shader(SDL_GPUDevice *device, const char *path, SDL_GPUShaderStage stage, uint32_t num_uniform_buffers)
{
    size_t code_size = 0;
    void *code = read_entire_file(path, &code_size);
    if (!code)
    {
        return NULL;
    }

    SDL_GPUShaderCreateInfo ci;
    SDL_zero(ci);
    ci.stage = stage;
    ci.format = SDL_GPU_SHADERFORMAT_SPIRV;
    ci.code = code;
    ci.code_size = (Uint32)code_size;
    ci.entrypoint = "main";
    ci.num_samplers = 0;
    ci.num_storage_textures = 0;
    ci.num_storage_buffers = 0;
    ci.num_uniform_buffers = num_uniform_buffers;

    SDL_GPUShader *s = SDL_CreateGPUShader(device, &ci);
    free(code);
    return s;
}

