
-- `root_path` is defined in the global table in C code, and contains
-- the path of the root configuration directory. `shader_mod` is the
-- name of the currently selected GLSL module.
package.path = package.path
    .. ";" .. root_path .. "/?/init.lua"
    .. ";" .. root_path .. "/?.lua"
    .. ";" .. root_path .. "/util/?.lua"

-- called when the OpenGL context is properly initialized
function setup()
    ui.faces = {
        ["default"] = ui.font("/usr/share/fonts/TTF/DejaVuSansMono.ttf", 40)
    }
    ui.faces["default"]:select()
end

local count = 0
local offset_x = 1
local offset_y = 1

function draw()
    local layer = ui.layer(600, 300)
    layer:position(20, 20)
    local function layer_render()
        
    end
    local function layer_fonts()
        local text = ui.text()
        text:position(20 + offset_x, 20 + offset_y)
        text:contents("Hello World!", { r = 1.0 })
        text:draw()
    end
    layer:handlers(layer_render, layer_fonts)
    layer:draw_contents()
    layer:draw()

    count = count + 1
    if count == 144 then
        offset_x = offset_x + 1
        offset_y = offset_y + 1
        count = 0
    end
end

local request_handlers = {
    mod = function(name)
        package.path = package.path
            .. ";" .. root_path .. "/" .. name  .. "/?/init.lua"
            .. ";" .. root_path .. "/" .. name  .. "/?.lua"
    end
}

function request(name, ...)
    local f = request_handlers[name]
    if f then f(...) end
end
