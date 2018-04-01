
-- `root_path` is defined in the global table in C code, and contains
-- the path of the root configuration directory. `shader_mod` is the
-- name of the currently selected GLSL module.
package.path = package.path
    .. ";" .. root_path .. "/?/init.lua"
    .. ";" .. root_path .. "/?.lua"
    .. ";" .. root_path .. "/util/?.lua"

draw = function() end

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
