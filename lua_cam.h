#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mat4.h"

typedef struct lua_State lua_State;

typedef struct LuaCam
{
    lua_State *L;
    int update_ref;
    int state_ref;

    int inotify_fd;
    int watch_fd;
    char script_path[256];
    char watch_dir[256];
    char watch_name[128];

    uint64_t prev_keys;
} LuaCam;

enum
{
    CAMKEY_LEFT  = 1ull << 0,
    CAMKEY_RIGHT = 1ull << 1,
    CAMKEY_UP    = 1ull << 2,
    CAMKEY_DOWN  = 1ull << 3,
    CAMKEY_SPACE = 1ull << 4,
    CAMKEY_LCTRL = 1ull << 5,
    CAMKEY_LSHIFT = 1ull << 6,
    CAMKEY_TAB   = 1ull << 7,
    CAMKEY_ESC   = 1ull << 8,
};

bool luacam_init(LuaCam *cam, const char *script_path);
void luacam_shutdown(LuaCam *cam);

bool luacam_reload_if_needed(LuaCam *cam);

bool luacam_update(
    LuaCam *cam,
    uint64_t keys_mask,
    float mouse_dx,
    float mouse_dy,
    float dt_s,
    mat4 *out_view,
    bool *out_mouse_look,
    bool *out_quit
);

