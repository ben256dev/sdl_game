#include "image.h"

#include <string.h>

#include "stb_image.h"

bool gpu_load_texture_rgba8(SDL_GPUDevice *device, const char *path, SDL_GPUTexture **out_tex, Uint32 *out_w, Uint32 *out_h)
{
    int w = 0;
    int h = 0;
    int n = 0;

    unsigned char *pixels = stbi_load(path, &w, &h, &n, 4);
    if (!pixels || w <= 0 || h <= 0)
        return false;

    SDL_GPUTextureCreateInfo tci;
    SDL_zero(tci);
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.width = (Uint32)w;
    tci.height = (Uint32)h;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    tci.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tci);
    if (!tex)
    {
        stbi_image_free(pixels);
        return false;
    }

    size_t size = (size_t)w * (size_t)h * 4;

    SDL_GPUTransferBufferCreateInfo tb_ci;
    SDL_zero(tb_ci);
    tb_ci.size = (Uint32)size;
    tb_ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer(device, &tb_ci);
    if (!tb)
    {
        SDL_ReleaseGPUTexture(device, tex);
        stbi_image_free(pixels);
        return false;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, tb, false);
    if (!mapped)
    {
        SDL_ReleaseGPUTransferBuffer(device, tb);
        SDL_ReleaseGPUTexture(device, tex);
        stbi_image_free(pixels);
        return false;
    }

    memcpy(mapped, pixels, size);
    SDL_UnmapGPUTransferBuffer(device, tb);
    stbi_image_free(pixels);

    SDL_GPUCommandBuffer *cb = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cb);

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = tb;
    src.offset = 0;
    src.pixels_per_row = (Uint32)w;
    src.rows_per_layer = (Uint32)h;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = tex;
    dst.mip_level = 0;
    dst.layer = 0;
    dst.x = 0;
    dst.y = 0;
    dst.z = 0;
    dst.w = (Uint32)w;
    dst.h = (Uint32)h;
    dst.d = 1;

    SDL_UploadToGPUTexture(cp, &src, &dst, false);

    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cb);
    SDL_WaitForGPUIdle(device);

    SDL_ReleaseGPUTransferBuffer(device, tb);

    *out_tex = tex;
    if (out_w) *out_w = (Uint32)w;
    if (out_h) *out_h = (Uint32)h;
    return true;
}

SDL_GPUSampler *gpu_create_sampler_repeat_linear(SDL_GPUDevice *device)
{
    SDL_GPUSamplerCreateInfo sci;
    SDL_zero(sci);
    sci.min_filter = SDL_GPU_FILTER_LINEAR;
    sci.mag_filter = SDL_GPU_FILTER_LINEAR;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    return SDL_CreateGPUSampler(device, &sci);
}

