local m3d = require("m3d")
local vec3 = m3d.vec3

function init()
    return {
        pos = vec3(0.0, 0.0, 3.0),
        yaw = 0.0,
        pitch = 0.0,
        mouse_look = false,
        sens = 0.0025,
        speed = 5.0,
        quit = false,
    }
end

function update(state, input, dt)
    if (input.pressed & KEY.TAB) ~= 0 then
        state.mouse_look = not state.mouse_look
    end

    if (input.pressed & KEY.ESC) ~= 0 then
        state.quit = true
    end

    if state.mouse_look then
        state.yaw = state.yaw + input.mouse_dx * state.sens
        state.pitch = state.pitch - input.mouse_dy * state.sens

        local lim = 1.553343
        if state.pitch < -lim then state.pitch = -lim end
        if state.pitch >  lim then state.pitch =  lim end
    end

    local cy = math.cos(state.yaw)
    local sy = math.sin(state.yaw)
    local cp = math.cos(state.pitch)
    local sp = math.sin(state.pitch)

    local forward = vec3(cp * sy, sp, -cp * cy):norm()
    local world_up = vec3(0.0, 1.0, 0.0)
    local right = forward:cross(world_up):norm()
    local up = right:cross(forward)

    local move = vec3(0.0, 0.0, 0.0)

    if (input.keys & KEY.LEFT) ~= 0 then move = move - right end
    if (input.keys & KEY.RIGHT) ~= 0 then move = move + right end
    if (input.keys & KEY.UP) ~= 0 then move = move + forward end
    if (input.keys & KEY.DOWN) ~= 0 then move = move - forward end
    if (input.keys & KEY.SPACE) ~= 0 then move = move + world_up end
    if (input.keys & KEY.LCTRL) ~= 0 then move = move - world_up end

    if move.x ~= 0.0 or move.y ~= 0.0 or move.z ~= 0.0 then
        move = move:norm()
        state.pos = state.pos + move * (state.speed * dt)
    end

    local center = state.pos + forward
    return m3d.mat4_lookat(state.pos, center, up)
end

