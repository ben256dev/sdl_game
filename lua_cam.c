#include "lua_cam.h"

#include <SDL3/SDL.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/inotify.h>

#if defined(__linux__)
#include <lua5.4/lua.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>
#else
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#endif

typedef struct LuaVec3
{
    vec3 v;
} LuaVec3;

typedef struct LuaMat4
{
    mat4 m;
} LuaMat4;

static LuaVec3 *luavec3_check(lua_State *L, int idx)
{
    return (LuaVec3 *)luaL_checkudata(L, idx, "m3d.vec3");
}

static LuaMat4 *luamat4_check(lua_State *L, int idx)
{
    return (LuaMat4 *)luaL_checkudata(L, idx, "m3d.mat4");
}

static int l_vec3_new(lua_State *L)
{
    float x = (float)luaL_optnumber(L, 1, 0.0);
    float y = (float)luaL_optnumber(L, 2, 0.0);
    float z = (float)luaL_optnumber(L, 3, 0.0);

    LuaVec3 *ud = (LuaVec3 *)lua_newuserdatauv(L, sizeof(*ud), 0);
    ud->v = (vec3){ x, y, z };

    luaL_getmetatable(L, "m3d.vec3");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_vec3_index(lua_State *L)
{
    LuaVec3 *ud = luavec3_check(L, 1);
    const char *k = luaL_checkstring(L, 2);

    if (k[0] == 'x' && k[1] == '\0')
    {
        lua_pushnumber(L, ud->v.x);
        return 1;
    }
    if (k[0] == 'y' && k[1] == '\0')
    {
        lua_pushnumber(L, ud->v.y);
        return 1;
    }
    if (k[0] == 'z' && k[1] == '\0')
    {
        lua_pushnumber(L, ud->v.z);
        return 1;
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    return 1;
}

static int l_vec3_newindex(lua_State *L)
{
    LuaVec3 *ud = luavec3_check(L, 1);
    const char *k = luaL_checkstring(L, 2);
    float v = (float)luaL_checknumber(L, 3);

    if (k[0] == 'x' && k[1] == '\0')
        ud->v.x = v;
    else if (k[0] == 'y' && k[1] == '\0')
        ud->v.y = v;
    else if (k[0] == 'z' && k[1] == '\0')
        ud->v.z = v;

    return 0;
}

static int l_vec3_add(lua_State *L)
{
    LuaVec3 *a = luavec3_check(L, 1);
    LuaVec3 *b = luavec3_check(L, 2);

    LuaVec3 *r = (LuaVec3 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->v = vec3_add(a->v, b->v);
    luaL_getmetatable(L, "m3d.vec3");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_vec3_sub(lua_State *L)
{
    LuaVec3 *a = luavec3_check(L, 1);
    LuaVec3 *b = luavec3_check(L, 2);

    LuaVec3 *r = (LuaVec3 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->v = vec3_sub(a->v, b->v);
    luaL_getmetatable(L, "m3d.vec3");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_vec3_unm(lua_State *L)
{
    LuaVec3 *a = luavec3_check(L, 1);

    LuaVec3 *r = (LuaVec3 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->v = (vec3){ -a->v.x, -a->v.y, -a->v.z };
    luaL_getmetatable(L, "m3d.vec3");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_vec3_mul(lua_State *L)
{
    if (luaL_testudata(L, 1, "m3d.vec3") && lua_isnumber(L, 2))
    {
        LuaVec3 *a = luavec3_check(L, 1);
        float s = (float)luaL_checknumber(L, 2);

        LuaVec3 *r = (LuaVec3 *)lua_newuserdatauv(L, sizeof(*r), 0);
        r->v = vec3_mul(a->v, s);
        luaL_getmetatable(L, "m3d.vec3");
        lua_setmetatable(L, -2);
        return 1;
    }

    if (lua_isnumber(L, 1) && luaL_testudata(L, 2, "m3d.vec3"))
    {
        float s = (float)luaL_checknumber(L, 1);
        LuaVec3 *a = luavec3_check(L, 2);

        LuaVec3 *r = (LuaVec3 *)lua_newuserdatauv(L, sizeof(*r), 0);
        r->v = vec3_mul(a->v, s);
        luaL_getmetatable(L, "m3d.vec3");
        lua_setmetatable(L, -2);
        return 1;
    }

    return luaL_error(L, "vec3 * expects (vec3, number) or (number, vec3)");
}

static int l_vec3_dot(lua_State *L)
{
    LuaVec3 *a = luavec3_check(L, 1);
    LuaVec3 *b = luavec3_check(L, 2);
    lua_pushnumber(L, vec3_dot(a->v, b->v));
    return 1;
}

static int l_vec3_cross(lua_State *L)
{
    LuaVec3 *a = luavec3_check(L, 1);
    LuaVec3 *b = luavec3_check(L, 2);

    LuaVec3 *r = (LuaVec3 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->v = vec3_cross(a->v, b->v);
    luaL_getmetatable(L, "m3d.vec3");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_vec3_norm(lua_State *L)
{
    LuaVec3 *a = luavec3_check(L, 1);

    LuaVec3 *r = (LuaVec3 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->v = vec3_norm(a->v);
    luaL_getmetatable(L, "m3d.vec3");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_mat4_identity(lua_State *L)
{
    LuaMat4 *r = (LuaMat4 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->m = mat4_identity();
    luaL_getmetatable(L, "m3d.mat4");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_mat4_mul(lua_State *L)
{
    LuaMat4 *a = luamat4_check(L, 1);
    LuaMat4 *b = luamat4_check(L, 2);

    LuaMat4 *r = (LuaMat4 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->m = mat4_mul(a->m, b->m);
    luaL_getmetatable(L, "m3d.mat4");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_mat4_translate(lua_State *L)
{
    LuaVec3 *t = luavec3_check(L, 1);

    LuaMat4 *r = (LuaMat4 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->m = mat4_translate(t->v);
    luaL_getmetatable(L, "m3d.mat4");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_mat4_rotate_x(lua_State *L)
{
    float a = (float)luaL_checknumber(L, 1);

    LuaMat4 *r = (LuaMat4 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->m = mat4_rotate_x(a);
    luaL_getmetatable(L, "m3d.mat4");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_mat4_rotate_y(lua_State *L)
{
    float a = (float)luaL_checknumber(L, 1);

    LuaMat4 *r = (LuaMat4 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->m = mat4_rotate_y(a);
    luaL_getmetatable(L, "m3d.mat4");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_mat4_perspective(lua_State *L)
{
    float fovy = (float)luaL_checknumber(L, 1);
    float aspect = (float)luaL_checknumber(L, 2);
    float znear = (float)luaL_checknumber(L, 3);
    float zfar = (float)luaL_checknumber(L, 4);

    LuaMat4 *r = (LuaMat4 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->m = mat4_perspective(fovy, aspect, znear, zfar);
    luaL_getmetatable(L, "m3d.mat4");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_mat4_lookat(lua_State *L)
{
    LuaVec3 *eye = luavec3_check(L, 1);
    LuaVec3 *center = luavec3_check(L, 2);
    LuaVec3 *up = luavec3_check(L, 3);

    LuaMat4 *r = (LuaMat4 *)lua_newuserdatauv(L, sizeof(*r), 0);
    r->m = mat4_lookat(eye->v, center->v, up->v);
    luaL_getmetatable(L, "m3d.mat4");
    lua_setmetatable(L, -2);
    return 1;
}

static int luaopen_m3d(lua_State *L)
{
    if (luaL_newmetatable(L, "m3d.vec3"))
    {
        lua_pushcfunction(L, l_vec3_index);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, l_vec3_newindex);
        lua_setfield(L, -2, "__newindex");

        lua_pushcfunction(L, l_vec3_add);
        lua_setfield(L, -2, "__add");

        lua_pushcfunction(L, l_vec3_sub);
        lua_setfield(L, -2, "__sub");

        lua_pushcfunction(L, l_vec3_mul);
        lua_setfield(L, -2, "__mul");

        lua_pushcfunction(L, l_vec3_unm);
        lua_setfield(L, -2, "__unm");

        lua_pushcfunction(L, l_vec3_dot);
        lua_setfield(L, -2, "dot");

        lua_pushcfunction(L, l_vec3_cross);
        lua_setfield(L, -2, "cross");

        lua_pushcfunction(L, l_vec3_norm);
        lua_setfield(L, -2, "norm");
    }
    lua_pop(L, 1);

    if (luaL_newmetatable(L, "m3d.mat4"))
    {
        lua_pushcfunction(L, l_mat4_mul);
        lua_setfield(L, -2, "__mul");
    }
    lua_pop(L, 1);

    lua_newtable(L);

    lua_pushcfunction(L, l_vec3_new);
    lua_setfield(L, -2, "vec3");

    lua_pushcfunction(L, l_mat4_identity);
    lua_setfield(L, -2, "mat4_identity");

    lua_pushcfunction(L, l_mat4_translate);
    lua_setfield(L, -2, "mat4_translate");

    lua_pushcfunction(L, l_mat4_rotate_x);
    lua_setfield(L, -2, "mat4_rotate_x");

    lua_pushcfunction(L, l_mat4_rotate_y);
    lua_setfield(L, -2, "mat4_rotate_y");

    lua_pushcfunction(L, l_mat4_perspective);
    lua_setfield(L, -2, "mat4_perspective");

    lua_pushcfunction(L, l_mat4_lookat);
    lua_setfield(L, -2, "mat4_lookat");

    lua_pushcfunction(L, l_mat4_mul);
    lua_setfield(L, -2, "mat4_mul");

    return 1;
}

static void split_path(const char *path, char *out_dir, size_t out_dir_cap, char *out_name, size_t out_name_cap)
{
    const char *slash = strrchr(path, '/');
    if (!slash)
    {
        snprintf(out_dir, out_dir_cap, "%s", ".");
        snprintf(out_name, out_name_cap, "%.*s", (int)(out_name_cap - 1), path);
        return;
    }

    size_t dlen = (size_t)(slash - path);
    if (dlen == 0)
        dlen = 1;

    if (dlen >= out_dir_cap)
        dlen = out_dir_cap - 1;

    memcpy(out_dir, path, dlen);
    out_dir[dlen] = '\0';

    snprintf(out_name, out_name_cap, "%.*s", (int)(out_name_cap - 1), slash + 1);
}

static void set_key_constants(lua_State *L)
{
    lua_newtable(L);

    lua_pushinteger(L, (lua_Integer)CAMKEY_LEFT);
    lua_setfield(L, -2, "LEFT");

    lua_pushinteger(L, (lua_Integer)CAMKEY_RIGHT);
    lua_setfield(L, -2, "RIGHT");

    lua_pushinteger(L, (lua_Integer)CAMKEY_UP);
    lua_setfield(L, -2, "UP");

    lua_pushinteger(L, (lua_Integer)CAMKEY_DOWN);
    lua_setfield(L, -2, "DOWN");

    lua_pushinteger(L, (lua_Integer)CAMKEY_SPACE);
    lua_setfield(L, -2, "SPACE");

    lua_pushinteger(L, (lua_Integer)CAMKEY_LCTRL);
    lua_setfield(L, -2, "LCTRL");

    lua_pushinteger(L, (lua_Integer)CAMKEY_LSHIFT);
    lua_setfield(L, -2, "LSHIFT");

    lua_pushinteger(L, (lua_Integer)CAMKEY_TAB);
    lua_setfield(L, -2, "TAB");

    lua_pushinteger(L, (lua_Integer)CAMKEY_ESC);
    lua_setfield(L, -2, "ESC");

    lua_setglobal(L, "KEY");
}

static bool load_script_and_cache_update(LuaCam *cam)
{
    if (luaL_dofile(cam->L, cam->script_path) != LUA_OK)
    {
        const char *err = lua_tostring(cam->L, -1);
        SDL_Log("lua: %s", err ? err : "unknown error");
        lua_pop(cam->L, 1);
        return false;
    }

    lua_getglobal(cam->L, "update");
    if (!lua_isfunction(cam->L, -1))
    {
        lua_pop(cam->L, 1);
        SDL_Log("lua: update is not a function");
        return false;
    }

    int new_ref = luaL_ref(cam->L, LUA_REGISTRYINDEX);

    if (cam->update_ref != LUA_NOREF && cam->update_ref != LUA_REFNIL)
        luaL_unref(cam->L, LUA_REGISTRYINDEX, cam->update_ref);

    cam->update_ref = new_ref;
    return true;
}

static bool call_init_for_state(LuaCam *cam)
{
    lua_getglobal(cam->L, "init");
    if (!lua_isfunction(cam->L, -1))
    {
        lua_pop(cam->L, 1);
        SDL_Log("lua: init is not a function");
        return false;
    }

    if (lua_pcall(cam->L, 0, 1, 0) != LUA_OK)
    {
        const char *err = lua_tostring(cam->L, -1);
        SDL_Log("lua: %s", err ? err : "unknown error");
        lua_pop(cam->L, 1);
        return false;
    }

    if (!lua_istable(cam->L, -1))
    {
        lua_pop(cam->L, 1);
        SDL_Log("lua: init must return a table");
        return false;
    }

    cam->state_ref = luaL_ref(cam->L, LUA_REGISTRYINDEX);
    return true;
}

bool luacam_init(LuaCam *cam, const char *script_path)
{
    memset(cam, 0, sizeof(*cam));
    cam->L = luaL_newstate();
    if (!cam->L)
        return false;

    luaL_openlibs(cam->L);

    luaL_requiref(cam->L, "m3d", luaopen_m3d, 1);
    lua_pop(cam->L, 1);

    set_key_constants(cam->L);

    snprintf(cam->script_path, sizeof(cam->script_path), "%s", script_path);
    split_path(cam->script_path, cam->watch_dir, sizeof(cam->watch_dir), cam->watch_name, sizeof(cam->watch_name));

    cam->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (cam->inotify_fd < 0)
    {
        SDL_Log("inotify_init1 failed: %s", strerror(errno));
        luacam_shutdown(cam);
        return false;
    }

    cam->watch_fd = inotify_add_watch(
        cam->inotify_fd,
        cam->watch_dir,
        IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE
    );

    if (cam->watch_fd < 0)
    {
        SDL_Log("inotify_add_watch failed: %s", strerror(errno));
        luacam_shutdown(cam);
        return false;
    }

    cam->update_ref = LUA_NOREF;
    cam->state_ref = LUA_NOREF;

    if (!load_script_and_cache_update(cam))
    {
        luacam_shutdown(cam);
        return false;
    }

    if (!call_init_for_state(cam))
    {
        luacam_shutdown(cam);
        return false;
    }

    cam->prev_keys = 0;
    return true;
}

void luacam_shutdown(LuaCam *cam)
{
    if (!cam)
        return;

    if (cam->L)
    {
        if (cam->update_ref != LUA_NOREF && cam->update_ref != LUA_REFNIL)
            luaL_unref(cam->L, LUA_REGISTRYINDEX, cam->update_ref);

        if (cam->state_ref != LUA_NOREF && cam->state_ref != LUA_REFNIL)
            luaL_unref(cam->L, LUA_REGISTRYINDEX, cam->state_ref);

        lua_close(cam->L);
        cam->L = NULL;
    }

    if (cam->watch_fd > 0 && cam->inotify_fd > 0)
    {
        inotify_rm_watch(cam->inotify_fd, cam->watch_fd);
        cam->watch_fd = -1;
    }

    if (cam->inotify_fd > 0)
    {
        close(cam->inotify_fd);
        cam->inotify_fd = -1;
    }
}

bool luacam_reload_if_needed(LuaCam *cam)
{
    if (!cam || cam->inotify_fd < 0)
        return false;

    struct pollfd pfd;
    pfd.fd = cam->inotify_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int pr = poll(&pfd, 1, 0);
    if (pr <= 0 || !(pfd.revents & POLLIN))
        return false;

    bool should_reload = false;

    char buf[4096];
    for (;;)
    {
        ssize_t n = read(cam->inotify_fd, buf, sizeof(buf));
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            break;
        }
        if (n == 0)
            break;

        size_t off = 0;
        while (off + sizeof(struct inotify_event) <= (size_t)n)
        {
            struct inotify_event *ev = (struct inotify_event *)(buf + off);
            if (ev->len > 0 && strcmp(ev->name, cam->watch_name) == 0)
                should_reload = true;
            off += sizeof(struct inotify_event) + ev->len;
        }
    }

    if (!should_reload)
        return false;

    int old_ref = cam->update_ref;
    if (!load_script_and_cache_update(cam))
    {
        cam->update_ref = old_ref;
        return false;
    }

    return true;
}

static void push_input_table(lua_State *L, uint64_t keys_mask, uint64_t pressed, uint64_t released, float mouse_dx, float mouse_dy)
{
    lua_createtable(L, 0, 5);

    lua_pushinteger(L, (lua_Integer)keys_mask);
    lua_setfield(L, -2, "keys");

    lua_pushinteger(L, (lua_Integer)pressed);
    lua_setfield(L, -2, "pressed");

    lua_pushinteger(L, (lua_Integer)released);
    lua_setfield(L, -2, "released");

    lua_pushnumber(L, mouse_dx);
    lua_setfield(L, -2, "mouse_dx");

    lua_pushnumber(L, mouse_dy);
    lua_setfield(L, -2, "mouse_dy");
}

static bool read_state_bool(lua_State *L, int state_index, const char *field, bool *out)
{
    lua_getfield(L, state_index, field);
    if (lua_isboolean(L, -1))
    {
        *out = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return true;
    }
    lua_pop(L, 1);
    return false;
}

bool luacam_update(
    LuaCam *cam,
    uint64_t keys_mask,
    float mouse_dx,
    float mouse_dy,
    float dt_s,
    mat4 *out_view,
    bool *out_mouse_look,
    bool *out_quit
)
{
    if (!cam || !cam->L)
        return false;

    uint64_t pressed = keys_mask & ~cam->prev_keys;
    uint64_t released = ~keys_mask & cam->prev_keys;
    cam->prev_keys = keys_mask;

    lua_rawgeti(cam->L, LUA_REGISTRYINDEX, cam->update_ref);
    lua_rawgeti(cam->L, LUA_REGISTRYINDEX, cam->state_ref);
    push_input_table(cam->L, keys_mask, pressed, released, mouse_dx, mouse_dy);
    lua_pushnumber(cam->L, dt_s);

    if (lua_pcall(cam->L, 3, 1, 0) != LUA_OK)
    {
        const char *err = lua_tostring(cam->L, -1);
        SDL_Log("lua: %s", err ? err : "unknown error");
        lua_pop(cam->L, 1);
        return false;
    }

    bool ok = false;
    if (luaL_testudata(cam->L, -1, "m3d.mat4"))
    {
        LuaMat4 *vm = luamat4_check(cam->L, -1);
        *out_view = vm->m;
        ok = true;
    }
    lua_pop(cam->L, 1);

    lua_rawgeti(cam->L, LUA_REGISTRYINDEX, cam->state_ref);
    int st = lua_gettop(cam->L);

    if (out_mouse_look)
    {
        bool ml = false;
        if (read_state_bool(cam->L, st, "mouse_look", &ml))
            *out_mouse_look = ml;
    }

    if (out_quit)
    {
        bool q = false;
        if (read_state_bool(cam->L, st, "quit", &q))
            *out_quit = q;
        else
            *out_quit = false;
    }

    lua_pop(cam->L, 1);
    return ok;
}

