#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "vec2.h"
#include "mat4.h"
#include "shader.h"
#include "log.h"

typedef struct Vertex
{
    float px, py, pz;
    float cr, cg, cb;
} Vertex;

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

    uint64_t t0 = SDL_GetTicksNS();

    int mouse_look_on = 0;
    float yaw = 0.0f;
    float pitch = 0.0f;
#define MOUSE_SENSITIVITY 0.0001f
#define CAMERA_MOVE_SPEED 0.1f
    SDL_SetWindowRelativeMouseMode(window, mouse_look_on);

    vec3 camera_position = VEC3_ZERO;

    int running = 1;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
            {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_TAB)
            {
                mouse_look_on = !mouse_look_on;
                SDL_SetWindowRelativeMouseMode(window, mouse_look_on);
            }
            if (mouse_look_on && event.type == SDL_EVENT_MOUSE_MOTION)
            {
                yaw += (float)event.motion.xrel * MOUSE_SENSITIVITY;
                pitch -= (float)event.motion.yrel * MOUSE_SENSITIVITY;

                float lim = 1.553343f;
                if (pitch < -lim) pitch = -lim;
                if (pitch >  lim) pitch =  lim;
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
        mat4 proj = mat4_perspective(45.0f * (PI / 180.0f), aspect, 0.1f, 100.0f);

        vec3 eye = { 0.0f, 0.0f, 3.0f };
        vec3 world_up = { 0.0f, 1.0f, 0.0f };

        vec3 forward = {
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            -cosf(pitch) * cosf(yaw),
        };
        forward = vec3_norm(forward);

        vec3 right = vec3_norm(vec3_cross(forward, world_up));
        vec3 cam_up = vec3_cross(right, forward);

        vec3 center = vec3_add(eye, forward);

        vec3 move_input_vector = VEC2_ZERO;
        const bool *keys = SDL_GetKeyboardState(NULL);
        move_input_vector.x = keys[SDL_SCANCODE_LEFT] - keys[SDL_SCANCODE_RIGHT];
        move_input_vector.z = keys[SDL_SCANCODE_DOWN] - keys[SDL_SCANCODE_UP];
        move_input_vector.y = keys[SDL_SCANCODE_LCTRL] - keys[SDL_SCANCODE_SPACE];

        vec3 move_right = vec3_mul(right, move_input_vector.x);
        vec3 move_forward = vec3_mul(forward, move_input_vector.z);
        vec3 move_up = vec3_mul(world_up, move_input_vector.y);

        camera_position = vec3_add(camera_position, vec3_mul(move_right, CAMERA_MOVE_SPEED));
        camera_position = vec3_add(camera_position, vec3_mul(move_forward, CAMERA_MOVE_SPEED));
        camera_position = vec3_add(camera_position, vec3_mul(move_up, CAMERA_MOVE_SPEED));

        mat4 look = mat4_lookat(eye, center, cam_up);
        mat4 move = mat4_translate(camera_position);
        mat4 view = mat4_mul(look, move);

        mat4 model = mat4_mul(mat4_rotate_y(time_s), mat4_rotate_x(time_s * 0.7f));
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
