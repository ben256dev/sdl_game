#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <stdio.h>
#include <stdlib.h>

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
