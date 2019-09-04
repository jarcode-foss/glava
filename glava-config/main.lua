local window = require("glava-config.window")

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

local main = {}

function main.entry(prog, ...)
    dependency("lgi")
    dependency("lfs")
    if glava.resource_path:sub(glava.resource_path:len()) ~= "/" then
      glava.resource_path = glava.resource_path .. "/"
    end
    window()
end

return main
