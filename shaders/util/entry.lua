
-- `root_path` is defined in the global table in C code, and contains
-- the path of the root configuration directory. `shader_mod` is the
-- name of the currently selected GLSL module.
package.path = package.path
    .. ";" .. root_path .. "/?/init.lua"
    .. ";" .. root_path .. "/?.lua"
    .. ";" .. root_path .. "/util/?.lua"

-- called when the OpenGL context is properly initialized
function setup()
    
end

function draw()
    local layer = ui.layer(200, 300)
    layer:position(20, 20)
    local function layer_render()
        
    end
    local function layer_fonts()
        local text = ui.text()
        text:position(20, 20)
        text:contents("Hello World!", { r = 1.0 })
        text:draw()
    end
    layer:handlers(layer_render, layer_fonts)
    layer:draw_contents()
    layer:draw()
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
