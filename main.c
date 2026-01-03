// main.c
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vec2.h"
#include "vec3.h"
#include "mat4.h"
#include "shader.h"
#include "log.h"
#include "lua_cam.h"
#include "image.h"

SDL_GPUDevice*  sdl_device;
SDL_GPUTexture* sdl_depth_tex = NULL;
SDL_GPUTexture* sdl_tex = NULL;
SDL_GPUSampler* sdl_samp = NULL;

#define SDL_HANDLE_MAX_COUNT 10
SDL_GPUGraphicsPipeline* sdl_pipelines[SDL_HANDLE_MAX_COUNT];        int sdl_pipelines_index = -1;
SDL_GPUTransferBuffer*   sdl_transfer_buffers[SDL_HANDLE_MAX_COUNT]; int sdl_transfer_buffers_index = -1;
SDL_GPUBuffer*           sdl_buffers[SDL_HANDLE_MAX_COUNT*2];        int sdl_buffers_index = -1;
SDL_GPUShader*           sdl_shaders[SDL_HANDLE_MAX_COUNT*2];        int sdl_shaders_index = -1;
SDL_Window*              sdl_windows[SDL_HANDLE_MAX_COUNT];          int sdl_windows_index = -1;

#define SDL_PUSH_PIPELINE(p)        (sdl_pipelines[++sdl_pipelines_index] = (p))
#define SDL_PUSH_TRANSFER_BUFFER(b) (sdl_transfer_buffers[++sdl_transfer_buffers_index] = (b))
#define SDL_PUSH_BUFFER(b)          (sdl_buffers[++sdl_buffers_index] = (b))
#define SDL_PUSH_SHADER(s)          (sdl_shaders[++sdl_shaders_index] = (s))
#define SDL_PUSH_WINDOW(w)          (sdl_windows[++sdl_windows_index] = (w))

void sdl_cleanup(void)
{
    if (sdl_device)
        SDL_WaitForGPUIdle(sdl_device);

    if (sdl_samp)
        SDL_ReleaseGPUSampler(sdl_device, sdl_samp);

    if (sdl_tex)
        SDL_ReleaseGPUTexture(sdl_device, sdl_tex);

    if (sdl_depth_tex)
        SDL_ReleaseGPUTexture(sdl_device, sdl_depth_tex);

    for (; sdl_pipelines_index > -1 ; sdl_pipelines_index--)
        SDL_ReleaseGPUGraphicsPipeline(sdl_device, sdl_pipelines[sdl_pipelines_index]);

    for (; sdl_transfer_buffers_index > -1 ; sdl_transfer_buffers_index--)
        SDL_ReleaseGPUTransferBuffer(sdl_device, sdl_transfer_buffers[sdl_transfer_buffers_index]);

    for (; sdl_buffers_index > -1 ; sdl_buffers_index--)
        SDL_ReleaseGPUBuffer(sdl_device, sdl_buffers[sdl_buffers_index]);

    for (; sdl_shaders_index > -1 ; sdl_shaders_index--)
        SDL_ReleaseGPUShader(sdl_device, sdl_shaders[sdl_shaders_index]);

    int w = sdl_windows_index;
    for (; w > -1 ; w--)
        SDL_ReleaseWindowFromGPUDevice(sdl_device, sdl_windows[w]);

    if (sdl_device)
        SDL_DestroyGPUDevice(sdl_device);

    for (; sdl_windows_index > -1 ; sdl_windows_index--)
        SDL_DestroyWindow(sdl_windows[sdl_windows_index]);

    SDL_Quit();
}

typedef struct Vertex
{
    float px, py, pz;
    float cr, cg, cb;
    float u, v;
} Vertex;

static uint64_t build_key_mask(void)
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    uint64_t m = 0;

    if (keys[SDL_SCANCODE_LEFT])  m |= CAMKEY_LEFT;
    if (keys[SDL_SCANCODE_RIGHT]) m |= CAMKEY_RIGHT;
    if (keys[SDL_SCANCODE_UP])    m |= CAMKEY_UP;
    if (keys[SDL_SCANCODE_DOWN])  m |= CAMKEY_DOWN;
    if (keys[SDL_SCANCODE_SPACE]) m |= CAMKEY_SPACE;
    if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]) m |= CAMKEY_LCTRL;
    if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) m |= CAMKEY_LSHIFT;
    if (keys[SDL_SCANCODE_TAB])   m |= CAMKEY_TAB;
    if (keys[SDL_SCANCODE_ESCAPE]) m |= CAMKEY_ESC;

    return m;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    SDL_SetLogOutputFunction(sdl_log_cb, NULL);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "SDL_Init failed: '%s'\n", SDL_GetError());
        dump_env();
        dump_video_drivers();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL3 GPU Grass Plane", 900, 600, SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow failed: '%s'\n", SDL_GetError());
        dump_env();
        sdl_cleanup();
        return 1;
    }
    SDL_PUSH_WINDOW(window);

    sdl_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    if (!sdl_device)
    {
        fprintf(stderr, "SDL_CreateGPUDevice failed: '%s'\n", SDL_GetError());
        sdl_cleanup();
        return 1;
    }

    if (!SDL_ClaimWindowForGPUDevice(sdl_device, window))
    {
        fprintf(stderr, "SDL_ClaimWindowForGPUDevice failed: '%s'\n", SDL_GetError());
        sdl_cleanup();
        return 1;
    }

    SDL_GPUTextureFormat swap_format = SDL_GetGPUSwapchainTextureFormat(sdl_device, window);

    SDL_GPUShader *vs = load_spirv_shader(sdl_device, "cube.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 1);
    SDL_GPUShader *fs = load_spirv_shader(sdl_device, "cube.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 0);
    SDL_PUSH_SHADER(vs);
    SDL_PUSH_SHADER(fs);
    if (!vs || !fs)
    {
        fprintf(stderr, "Failed to load shaders. Need cube.vert.spv and cube.frag.spv\n");
        sdl_cleanup();
        return 1;
    }

    if (!gpu_load_texture_rgba8(sdl_device, "assets/grass.jpg", &sdl_tex, NULL, NULL))
    {
        fprintf(stderr, "Failed to load assets/grass.jpg\n");
        sdl_cleanup();
        return 1;
    }

    sdl_samp = gpu_create_sampler_repeat_linear(sdl_device);
    if (!sdl_samp)
    {
        fprintf(stderr, "Failed to create sampler: '%s'\n", SDL_GetError());
        sdl_cleanup();
        return 1;
    }

    const float PLANE_SIZE = 50.0f;
    const float PLANE_TILES = 16.0f;

    Vertex verts[] = {
        { -PLANE_SIZE, 0.0f, -PLANE_SIZE,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f },
        {  PLANE_SIZE, 0.0f, -PLANE_SIZE,  1.0f, 1.0f, 1.0f,  PLANE_TILES, 0.0f },
        {  PLANE_SIZE, 0.0f,  PLANE_SIZE,  1.0f, 1.0f, 1.0f,  PLANE_TILES, PLANE_TILES },
        { -PLANE_SIZE, 0.0f,  PLANE_SIZE,  1.0f, 1.0f, 1.0f,  0.0f, PLANE_TILES },
    };

    uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };

    SDL_GPUBufferCreateInfo vb_ci;
    SDL_zero(vb_ci);
    vb_ci.size = (Uint32)sizeof(verts);
    vb_ci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    SDL_GPUBuffer *vb = SDL_CreateGPUBuffer(sdl_device, &vb_ci);
    SDL_PUSH_BUFFER(vb);

    SDL_GPUBufferCreateInfo ib_ci;
    SDL_zero(ib_ci);
    ib_ci.size = (Uint32)sizeof(indices);
    ib_ci.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    SDL_GPUBuffer *ib = SDL_CreateGPUBuffer(sdl_device, &ib_ci);
    SDL_PUSH_BUFFER(ib);

    if (!vb || !ib)
    {
        fprintf(stderr, "Failed to create GPU buffers: '%s'\n", SDL_GetError());
        sdl_cleanup();
        return 1;
    }

    SDL_GPUTransferBufferCreateInfo tb_ci;
    SDL_zero(tb_ci);
    tb_ci.size = (Uint32)(sizeof(verts) + sizeof(indices));
    tb_ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer *upload_tb = SDL_CreateGPUTransferBuffer(sdl_device, &tb_ci);
    SDL_PUSH_TRANSFER_BUFFER(upload_tb);

    if (!upload_tb)
    {
        fprintf(stderr, "Failed to create transfer buffer: '%s'\n", SDL_GetError());
        sdl_cleanup();
        return 1;
    }

    void *mapped = SDL_MapGPUTransferBuffer(sdl_device, upload_tb, false);
    if (!mapped)
    {
        fprintf(stderr, "Failed to map transfer buffer: '%s'\n", SDL_GetError());
        sdl_cleanup();
        return 1;
    }

    memcpy(mapped, verts, sizeof(verts));
    memcpy((uint8_t *)mapped + sizeof(verts), indices, sizeof(indices));
    SDL_UnmapGPUTransferBuffer(sdl_device, upload_tb);

    SDL_GPUCommandBuffer *init_cb = SDL_AcquireGPUCommandBuffer(sdl_device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(init_cb);

    SDL_GPUTransferBufferLocation src_v;
    src_v.transfer_buffer = upload_tb;
    src_v.offset = 0;

    SDL_GPUTransferBufferLocation src_i;
    src_i.transfer_buffer = upload_tb;
    src_i.offset = (Uint32)sizeof(verts);

    SDL_GPUBufferRegion dst_v;
    dst_v.buffer = vb;
    dst_v.offset = 0;
    dst_v.size = (Uint32)sizeof(verts);

    SDL_GPUBufferRegion dst_i;
    dst_i.buffer = ib;
    dst_i.offset = 0;
    dst_i.size = (Uint32)sizeof(indices);

    SDL_UploadToGPUBuffer(copy, &src_v, &dst_v, false);
    SDL_UploadToGPUBuffer(copy, &src_i, &dst_i, false);

    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(init_cb);
    SDL_WaitForGPUIdle(sdl_device);

    SDL_GPUVertexBufferDescription vbuf_desc;
    SDL_zero(vbuf_desc);
    vbuf_desc.slot = 0;
    vbuf_desc.pitch = (Uint32)sizeof(Vertex);
    vbuf_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vbuf_desc.instance_step_rate = 0;

    SDL_GPUVertexAttribute attrs[3];
    SDL_zeroa(attrs);

    attrs[0].location = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset = 0;

    attrs[1].location = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset = (Uint32)(sizeof(float) * 3);

    attrs[2].location = 2;
    attrs[2].buffer_slot = 0;
    attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[2].offset = (Uint32)(sizeof(float) * 6);

    SDL_GPUVertexInputState vin;
    SDL_zero(vin);
    vin.num_vertex_buffers = 1;
    vin.vertex_buffer_descriptions = &vbuf_desc;
    vin.num_vertex_attributes = 3;
    vin.vertex_attributes = attrs;

    SDL_GPUColorTargetDescription cdesc;
    SDL_zero(cdesc);
    cdesc.format = swap_format;

    SDL_GPUGraphicsPipelineTargetInfo tgt;
    SDL_zero(tgt);
    tgt.num_color_targets = 1;
    tgt.color_target_descriptions = &cdesc;
    tgt.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    SDL_GPURasterizerState rast;
    SDL_zero(rast);
    rast.fill_mode = SDL_GPU_FILLMODE_FILL;
    rast.cull_mode = SDL_GPU_CULLMODE_BACK;
    rast.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

    SDL_GPUDepthStencilState ds;
    SDL_zero(ds);
    ds.enable_depth_test = true;
    ds.enable_depth_write = true;
    ds.compare_op = SDL_GPU_COMPAREOP_LESS;

    SDL_GPUMultisampleState ms;
    SDL_zero(ms);
    ms.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUGraphicsPipelineCreateInfo pso;
    SDL_zero(pso);
    pso.vertex_shader = vs;
    pso.fragment_shader = fs;
    pso.vertex_input_state = vin;
    pso.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pso.rasterizer_state = rast;
    pso.depth_stencil_state = ds;
    pso.multisample_state = ms;
    pso.target_info = tgt;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(sdl_device, &pso);
    if (!pipeline)
    {
        fprintf(stderr, "SDL_CreateGPUGraphicsPipeline failed: '%s'\n", SDL_GetError());
        sdl_cleanup();
        return 1;
    }
    SDL_PUSH_PIPELINE(pipeline);

    Uint32 depth_w = 0;
    Uint32 depth_h = 0;

    uint64_t t0 = SDL_GetTicksNS();
    uint64_t last_frame = t0;

    LuaCam cam;
    if (!luacam_init(&cam, "camera.lua"))
    {
        fprintf(stderr, "Failed to init Lua camera\n");
        sdl_cleanup();
        return 1;
    }

    bool mouse_look_on = false;
    SDL_SetWindowRelativeMouseMode(window, mouse_look_on);

    int running = 1;
    while (running)
    {
        luacam_reload_if_needed(&cam);

        float mouse_dx = 0.0f;
        float mouse_dy = 0.0f;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
                running = 0;

            if (event.type == SDL_EVENT_MOUSE_MOTION)
            {
                mouse_dx += (float)event.motion.xrel;
                mouse_dy += (float)event.motion.yrel;
            }
        }

        uint64_t now = SDL_GetTicksNS();
        float dt_s = (float)((double)(now - last_frame) / 1000000000.0);
        last_frame = now;

        SDL_GPUCommandBuffer *cb = SDL_AcquireGPUCommandBuffer(sdl_device);

        SDL_GPUTexture *swap_tex = NULL;
        Uint32 w = 0;
        Uint32 h = 0;

        if (!SDL_WaitAndAcquireGPUSwapchainTexture(cb, window, &swap_tex, &w, &h))
        {
            SDL_SubmitGPUCommandBuffer(cb);
            continue;
        }

        if (!sdl_depth_tex || w != depth_w || h != depth_h)
        {
            if (sdl_depth_tex)
            {
                SDL_ReleaseGPUTexture(sdl_device, sdl_depth_tex);
                sdl_depth_tex = NULL;
            }

            SDL_GPUTextureCreateInfo tci;
            SDL_zero(tci);
            tci.type = SDL_GPU_TEXTURETYPE_2D;
            tci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            tci.width = w;
            tci.height = h;
            tci.layer_count_or_depth = 1;
            tci.num_levels = 1;
            tci.sample_count = SDL_GPU_SAMPLECOUNT_1;

            sdl_depth_tex = SDL_CreateGPUTexture(sdl_device, &tci);
            depth_w = w;
            depth_h = h;

            if (!sdl_depth_tex)
            {
                fprintf(stderr, "Failed to create depth texture: '%s'\n", SDL_GetError());
                SDL_SubmitGPUCommandBuffer(cb);
                continue;
            }
        }

        float time_s = (float)((double)(now - t0) / 1000000000.0);
        float aspect = (h == 0) ? 1.0f : ((float)w / (float)h);

        const float PI = 3.14159265358979323846f;
        mat4 proj = mat4_perspective(45.0f * (PI / 180.0f), aspect, 0.1f, 500.0f);

        uint64_t keys_mask = build_key_mask();

        mat4 view = mat4_identity();
        bool want_mouse_look = mouse_look_on;
        bool want_quit = false;

        if (luacam_update(&cam, keys_mask, mouse_dx, mouse_dy, dt_s, &view, &want_mouse_look, &want_quit))
        {
            if (want_quit)
                running = 0;

            if (want_mouse_look != mouse_look_on)
            {
                mouse_look_on = want_mouse_look;
                SDL_SetWindowRelativeMouseMode(window, mouse_look_on);
            }
        }

        mat4 model = mat4_identity();
        mat4 mv = mat4_mul(view, model);
        mat4 mvp = mat4_mul(proj, mv);

        SDL_GPUColorTargetInfo color;
        SDL_zero(color);
        color.texture = swap_tex;
        color.clear_color.r = 0.08f;
        color.clear_color.g = 0.08f;
        color.clear_color.b = 0.10f;
        color.clear_color.a = 1.0f;
        color.load_op = SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo depth;
        SDL_zero(depth);
        depth.texture = sdl_depth_tex;
        depth.clear_depth = 1.0f;
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *rp = SDL_BeginGPURenderPass(cb, &color, 1, &depth);

        SDL_BindGPUGraphicsPipeline(rp, pipeline);

        SDL_GPUTextureSamplerBinding tsb;
        tsb.texture = sdl_tex;
        tsb.sampler = sdl_samp;
        SDL_BindGPUFragmentSamplers(rp, 0, &tsb, 1);

        SDL_GPUBufferBinding vbind;
        vbind.buffer = vb;
        vbind.offset = 0;
        SDL_BindGPUVertexBuffers(rp, 0, &vbind, 1);

        SDL_GPUBufferBinding ibind;
        ibind.buffer = ib;
        ibind.offset = 0;
        SDL_BindGPUIndexBuffer(rp, &ibind, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_PushGPUVertexUniformData(cb, 0, &mvp, (Uint32)sizeof(mvp));

        SDL_DrawGPUIndexedPrimitives(rp, 6, 1, 0, 0, 0);

        SDL_EndGPURenderPass(rp);

        SDL_SubmitGPUCommandBuffer(cb);
        (void)time_s;
    }

    luacam_shutdown(&cam);
    sdl_cleanup();
    return 0;
}

