local function dependency(name)
  if package.loaded[name] then
    return
  else
    for _, searcher in ipairs(package.searchers or package.loaders) do
      local loader = searcher(name)
      if type(loader) == 'function' then
        package.preload[name] = loader
        return
      end
    end
    print("Dependency \"" .. name .. "\" is not installed.")
    print("Please install it through your package manager or Lua distribution.")
    os.exit(1)
  end
end

function glava.fail(message)
  print(string.format("!!FATAL!!: %s", message))
  os.exit(1)
end

local main = {}

-- Format string, but silently return nil if varargs contains any nil entries
local function format_silent(fmt, ...)
  for _, v in ipairs({...}) do
    if v == nil then return nil end
  end
  return string.format(fmt, ...)
end

function main.entry(prog, ...)
    dependency("lgi")
    dependency("lfs")
    
    if glava.resource_path:sub(glava.resource_path:len()) ~= "/" then
      glava.resource_path = glava.resource_path .. "/"
    end
    glava.config_path = format_silent("%s/glava", os.getenv("XDG_CONFIG_HOME"))
      or format_silent("%s/.config/glava", os.getenv("HOME"))
      or "/home/.config/glava"
    
    local lfs    = require "lfs"
    local window = require "glava-config.window"
    
    glava.module_list = {}
    for m in lfs.dir(glava.system_shader_path) do
      if m ~= "." and m ~= ".."
        and lfs.attributes(glava.system_shader_path .. "/" .. m, "mode") == "directory"
      and m ~= "util" then
        glava.module_list[#glava.module_list + 1] = m
      end
    end
    
    local mappings = require "glava-config.mappings"
    -- Associate `map_name = tbl` from mapping list for future lookups, etc.
    for k, v in pairs(mappings) do
      local i = 1
      local adv = false
      while v[i] ~= nil do
        if type(v[i]) == "table" then
          v[v[i][1]] = v[i]
          v[i].advanced = adv
          i = i + 1
        elseif type(v[i]) == "string" and v[i] == "advanced" then
          adv = true
          table.remove(v, i)
        else
          glava.fail(string.format("Unknown mappings entry type for file: \"%s\"", type(v)))
        end
      end
    end

    -- Enter into Gtk window
    window()
end

return main
