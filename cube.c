#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Vec3
{
    float x, y, z;
} Vec3;

typedef struct Mat4
{
    float m[16];
} Mat4;

static void SDLCALL sdl_log_cb(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    (void)userdata;
    fprintf(stderr, "SDL[%d][%d] %s\n", category, (int)priority, message);
}

static void dump_env(void)
{
    const char *a = getenv("DISPLAY");
    const char *b = getenv("WAYLAND_DISPLAY");
    const char *c = getenv("XDG_SESSION_TYPE");
    const char *d = getenv("XDG_RUNTIME_DIR");
    fprintf(stderr, "ENV DISPLAY=%s\n", a ? a : "(null)");
    fprintf(stderr, "ENV WAYLAND_DISPLAY=%s\n", b ? b : "(null)");
    fprintf(stderr, "ENV XDG_SESSION_TYPE=%s\n", c ? c : "(null)");
    fprintf(stderr, "ENV XDG_RUNTIME_DIR=%s\n", d ? d : "(null)");
}

static void dump_video_drivers(void)
{
    int n = SDL_GetNumVideoDrivers();
    fprintf(stderr, "SDL_GetNumVideoDrivers=%d\n", n);
    for (int i = 0; i < n; i++)
    {
        const char *name = SDL_GetVideoDriver(i);
        fprintf(stderr, "  video_driver[%d]=%s\n", i, name ? name : "(null)");
    }
}

static Mat4 mat4_identity(void)
{
    Mat4 r;
    memset(&r, 0, sizeof(r));
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

static Mat4 mat4_mul(Mat4 a, Mat4 b)
{
    Mat4 r;
    for (int c = 0; c < 4; c++)
    {
        for (int row = 0; row < 4; row++)
        {
            r.m[c * 4 + row] =
                a.m[0 * 4 + row] * b.m[c * 4 + 0] +
                a.m[1 * 4 + row] * b.m[c * 4 + 1] +
                a.m[2 * 4 + row] * b.m[c * 4 + 2] +
                a.m[3 * 4 + row] * b.m[c * 4 + 3];
        }
    }
    return r;
}

static Mat4 mat4_rotate_x(float a)
{
    Mat4 r = mat4_identity();
    float c = cosf(a);
    float s = sinf(a);
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

static Mat4 mat4_rotate_y(float a)
{
    Mat4 r = mat4_identity();
    float c = cosf(a);
    float s = sinf(a);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

static Mat4 mat4_perspective(float fovy_radians, float aspect, float znear, float zfar)
{
    float f = 1.0f / tanf(fovy_radians * 0.5f);
    Mat4 r;
    memset(&r, 0, sizeof(r));
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

static Vec3 vec3_sub(Vec3 a, Vec3 b)
{
    Vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

static Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    Vec3 r = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return r;
}

static float vec3_dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 vec3_norm(Vec3 v)
{
    float len = sqrtf(vec3_dot(v, v));
    if (len <= 0.0f)
    {
        Vec3 z = { 0, 0, 0 };
        return z;
    }
    Vec3 r = { v.x / len, v.y / len, v.z / len };
    return r;
}

static Mat4 mat4_lookat(Vec3 eye, Vec3 center, Vec3 up)
{
    Vec3 f = vec3_norm(vec3_sub(center, eye));
    Vec3 s = vec3_norm(vec3_cross(f, up));
    Vec3 u = vec3_cross(s, f);

    Mat4 r = mat4_identity();
    r.m[0] = s.x;
    r.m[1] = u.x;
    r.m[2] = -f.x;

    r.m[4] = s.y;
    r.m[5] = u.y;
    r.m[6] = -f.y;

    r.m[8] = s.z;
    r.m[9] = u.z;
    r.m[10] = -f.z;

    r.m[12] = -vec3_dot(s, eye);
    r.m[13] = -vec3_dot(u, eye);
    r.m[14] = vec3_dot(f, eye);

    return r;
}

typedef struct Vertex
{
    float px, py, pz;
    float cr, cg, cb;
} Vertex;

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

    SDL_Window *window = SDL_CreateWindow("SDL3 GPU Rotating Cube", 900, 600, SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow failed: '%s'\n", SDL_GetError());
        dump_env();
        SDL_Quit();
        return 1;
    }

    SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    if (!device)
    {
        fprintf(stderr, "SDL_CreateGPUDevice failed: '%s'\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        fprintf(stderr, "SDL_ClaimWindowForGPUDevice failed: '%s'\n", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GPUTextureFormat swap_format = SDL_GetGPUSwapchainTextureFormat(device, window);

    SDL_GPUShader *vs = load_spirv_shader(device, "cube.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 1);
    SDL_GPUShader *fs = load_spirv_shader(device, "cube.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 0);
    if (!vs || !fs)
    {
        fprintf(stderr, "Failed to load shaders. Need cube.vert.spv and cube.frag.spv\n");
        if (vs) SDL_ReleaseGPUShader(device, vs);
        if (fs) SDL_ReleaseGPUShader(device, fs);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Vertex verts[] = {
        { -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f },
        { -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f },

        { -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f },
        {  0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f },
        {  0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f },
        { -0.5f,  0.5f, -0.5f,  0.2f, 0.2f, 0.2f },
    };

    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        1, 5, 6,  1, 6, 2,
        5, 4, 7,  5, 7, 6,
        4, 0, 3,  4, 3, 7,
        3, 2, 6,  3, 6, 7,
        4, 5, 1,  4, 1, 0
    };

    SDL_GPUBufferCreateInfo vb_ci;
    SDL_zero(vb_ci);
    vb_ci.size = (Uint32)sizeof(verts);
    vb_ci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    SDL_GPUBuffer *vb = SDL_CreateGPUBuffer(device, &vb_ci);

    SDL_GPUBufferCreateInfo ib_ci;
    SDL_zero(ib_ci);
    ib_ci.size = (Uint32)sizeof(indices);
    ib_ci.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    SDL_GPUBuffer *ib = SDL_CreateGPUBuffer(device, &ib_ci);

    if (!vb || !ib)
    {
        fprintf(stderr, "Failed to create GPU buffers: '%s'\n", SDL_GetError());
        if (vb) SDL_ReleaseGPUBuffer(device, vb);
        if (ib) SDL_ReleaseGPUBuffer(device, ib);
        SDL_ReleaseGPUShader(device, vs);
        SDL_ReleaseGPUShader(device, fs);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GPUTransferBufferCreateInfo tb_ci;
    SDL_zero(tb_ci);
    tb_ci.size = (Uint32)(sizeof(verts) + sizeof(indices));
    tb_ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer *upload_tb = SDL_CreateGPUTransferBuffer(device, &tb_ci);

    if (!upload_tb)
    {
        fprintf(stderr, "Failed to create transfer buffer: '%s'\n", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, vb);
        SDL_ReleaseGPUBuffer(device, ib);
        SDL_ReleaseGPUShader(device, vs);
        SDL_ReleaseGPUShader(device, fs);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, upload_tb, false);
    if (!mapped)
    {
        fprintf(stderr, "Failed to map transfer buffer: '%s'\n", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, upload_tb);
        SDL_ReleaseGPUBuffer(device, vb);
        SDL_ReleaseGPUBuffer(device, ib);
        SDL_ReleaseGPUShader(device, vs);
        SDL_ReleaseGPUShader(device, fs);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    memcpy(mapped, verts, sizeof(verts));
    memcpy((uint8_t *)mapped + sizeof(verts), indices, sizeof(indices));
    SDL_UnmapGPUTransferBuffer(device, upload_tb);

    SDL_GPUCommandBuffer *init_cb = SDL_AcquireGPUCommandBuffer(device);
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
    SDL_WaitForGPUIdle(device);

    SDL_GPUVertexBufferDescription vbuf_desc;
    SDL_zero(vbuf_desc);
    vbuf_desc.slot = 0;
    vbuf_desc.pitch = (Uint32)sizeof(Vertex);
    vbuf_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vbuf_desc.instance_step_rate = 0;

    SDL_GPUVertexAttribute attrs[2];
    SDL_zeroa(attrs);

    attrs[0].location = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[0].offset = 0;

    attrs[1].location = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset = (Uint32)(sizeof(float) * 3);

    SDL_GPUVertexInputState vin;
    SDL_zero(vin);
    vin.num_vertex_buffers = 1;
    vin.vertex_buffer_descriptions = &vbuf_desc;
    vin.num_vertex_attributes = 2;
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
    rast.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

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

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(device, &pso);
    if (!pipeline)
    {
        fprintf(stderr, "SDL_CreateGPUGraphicsPipeline failed: '%s'\n", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, upload_tb);
        SDL_ReleaseGPUBuffer(device, vb);
        SDL_ReleaseGPUBuffer(device, ib);
        SDL_ReleaseGPUShader(device, vs);
        SDL_ReleaseGPUShader(device, fs);
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GPUTexture *depth_tex = NULL;
    Uint32 depth_w = 0;
    Uint32 depth_h = 0;

    bool running = true;
    uint64_t t0 = SDL_GetTicksNS();

    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)
            {
                running = false;
            }
        }

        SDL_GPUCommandBuffer *cb = SDL_AcquireGPUCommandBuffer(device);

        SDL_GPUTexture *swap_tex = NULL;
        Uint32 w = 0;
        Uint32 h = 0;

        if (!SDL_WaitAndAcquireGPUSwapchainTexture(cb, window, &swap_tex, &w, &h))
        {
            SDL_SubmitGPUCommandBuffer(cb);
            continue;
        }

        if (!depth_tex || w != depth_w || h != depth_h)
        {
            if (depth_tex)
            {
                SDL_ReleaseGPUTexture(device, depth_tex);
                depth_tex = NULL;
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

            depth_tex = SDL_CreateGPUTexture(device, &tci);
            depth_w = w;
            depth_h = h;

            if (!depth_tex)
            {
                fprintf(stderr, "Failed to create depth texture: '%s'\n", SDL_GetError());
                SDL_SubmitGPUCommandBuffer(cb);
                continue;
            }
        }

        uint64_t now = SDL_GetTicksNS();
        float time_s = (float)((double)(now - t0) / 1000000000.0);

        float aspect = (h == 0) ? 1.0f : ((float)w / (float)h);

        const float PI = 3.14159265358979323846f;
        Mat4 proj = mat4_perspective(45.0f * (PI / 180.0f), aspect, 0.1f, 100.0f);

        Vec3 eye = { 0.0f, 0.0f, 3.0f };
        Vec3 center = { 0.0f, 0.0f, 0.0f };
        Vec3 up = { 0.0f, 1.0f, 0.0f };
        Mat4 view = mat4_lookat(eye, center, up);

        Mat4 model = mat4_mul(mat4_rotate_y(time_s), mat4_rotate_x(time_s * 0.7f));
        Mat4 mv = mat4_mul(view, model);
        Mat4 mvp = mat4_mul(proj, mv);

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
        depth.texture = depth_tex;
        depth.clear_depth = 1.0f;
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *rp = SDL_BeginGPURenderPass(cb, &color, 1, &depth);

        SDL_BindGPUGraphicsPipeline(rp, pipeline);

        SDL_GPUBufferBinding vbind;
        vbind.buffer = vb;
        vbind.offset = 0;
        SDL_BindGPUVertexBuffers(rp, 0, &vbind, 1);

        SDL_GPUBufferBinding ibind;
        ibind.buffer = ib;
        ibind.offset = 0;
        SDL_BindGPUIndexBuffer(rp, &ibind, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_PushGPUVertexUniformData(cb, 0, &mvp, (Uint32)sizeof(mvp));

        SDL_DrawGPUIndexedPrimitives(rp, 36, 1, 0, 0, 0);

        SDL_EndGPURenderPass(rp);

        SDL_SubmitGPUCommandBuffer(cb);
    }

    SDL_WaitForGPUIdle(device);

    if (depth_tex) SDL_ReleaseGPUTexture(device, depth_tex);
    SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
    SDL_ReleaseGPUTransferBuffer(device, upload_tb);
    SDL_ReleaseGPUBuffer(device, vb);
    SDL_ReleaseGPUBuffer(device, ib);
    SDL_ReleaseGPUShader(device, vs);
    SDL_ReleaseGPUShader(device, fs);

    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

