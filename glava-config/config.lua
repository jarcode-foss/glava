local lfs      = require "lfs"
local mappings = require "glava-config.mappings"

local config = {
  Profile = { mt = {} },
  PROFILES_DIR = "profiles"
}

config.Profile.__index = config.Profile
setmetatable(config.Profile, config.Profile.mt)

-- Split path into entries, such that `table.concat` can be used to
-- reconstruct the path. Prepends the result with an empty string so
-- root (absolute) paths are preserved
local function path_split(str, sep)
  local sep, fields = sep or ":", (str:sub(1, sep:len()) == sep and {""} or {})
   local pattern = string.format("([^%s]+)", sep)
   str:gsub(pattern, function(c) fields[#fields + 1] = c end)
   return fields
end

-- Concatenates paths such that duplicate path separators are removed.
-- Can be used on non-split arguments, and resolves `..` syntax
local function path_concat(...)
  local ret = {}
  for _, v in ipairs({...}) do
    for _, e in ipairs(path_split(v, "/")) do
      if e ~= "" or #ret == 0 then
        if e == ".." and #ret >= 1 then
          ret[#ret] = nil
        else
          ret[#ret + 1] = e
        end
      end
    end
  end
  return table.concat(ret, "/")
end

-- Wrap table such that it can be called to index and call its members,
-- useful for switch-style syntax
local function switch(tbl)
  local mt = { __call = function(self, i) return rawget(self, i)() end }
  return setmetatable(tbl, mt)
end

-- To parse data from GLSL configs we use some complex pattern matching.
--
-- Because Lua's patterns operate on a per-character basis and do not offer
-- any read-ahead functionality, we use a pattern 'replacement' functionality
-- such that the match of an input pattern is passed to a function to produce
-- an output pattern.
--
-- This effectively means we have some fairly powerful parsing which allows us
-- to handle things like quoted strings with escaped characters.
local function unquote(match)
  local ret = {}
  local escaped = false
  for c in match:gmatch(".") do
    if c == "\"" then
      if escaped then ret[#ret + 1] = c end
    elseif c ~= "\\" then ret[#ret + 1] = c end
    if c == "\\" then
      if escaped then ret[#ret + 1] = c end
      escaped = not escaped
    else escaped = false end
  end
  return table.concat(ret, "")
end
local function none(...) return ... end
local MATCH_ENTRY_PATTERN = "^%s*%#(%a+)%s+(%a+)"
local MATCH_DATA_PREFIX   = "^%s*%#%a+%s+%a+"
local MATCH_TYPES = {
  ["float"]      = { pattern  = "(%d+.?%d*)" },
  ["int"]        = { pattern  = "(%d+)"      },
  ["color-expr"] = { pattern  = "(.+)"       },
  ["expr"]       = { pattern  = "(.+)"       },
  ["ident"]      = { pattern  = "(%a%w*)"    },
  ["string"] = {
    pattern = "(.+)",
    cast = unquote,
    -- Read-ahead function to generate a fixed-width pattern
    -- to match the next (possibly quoted) string
    transform = function(match)
      local quoted  = false
      local start   = true
      local escaped = false
      local count   = 0
      local skip = 0
      for c in match:gmatch(".") do
        count = count + 1
        if c == "\"" then
          if start then
            start  = false
            quoted = true
          elseif not escaped then
            if quoted then
              -- End-quote; end of string
              break
            else
              -- Formatting error: non-escaped quote after string start: `foo"bar`
              -- We attempt to resolve this by halting parsing and skipping the
              -- out-of-context quotation
              count = count - 1
              skip = skip + 1
              break
            end
          end
        elseif c == " " then
          if not start and not quoted then
            -- Un-escaped space; end of string
            -- skip the space itself
            count = count - 1
            break
          end
        else start = false end
        if c == "\\" then
          escaped = not escaped
        else escaped = false end
      end
      -- Strings without an ending quote will simply take up the remainder of
      -- the request, causing the following arguments to be overwritten. This
      -- is intended to ensure we can save valid options after stripping out
      -- the errornous quotes and using defaults for the subsequent arguments.
      local ret = { "(" }
      for t = 1, count do
        ret[1 + t] = "."
      end
      ret[2 + count] = ")"
      for t = 1, skip do
        ret[2 + count + t] = "."
      end
      return table.concat(ret, "")
    end,
    serialize = function(x)
      return string.format("\"%s\"", x)
    end
  }
}

config.path_concat = path_concat
config.path_split  = path_split

local function create_pf(arr, mode, silent)
  local parts = {}
  local function errfmt(err)
    return string.format("Failed to create '%s' in '%s': %s",
                         path_concat(parts, "/"), path_concat(arr, "/"), err)
  end
  for i, v in ipairs(arr) do
    parts[#parts + 1] = v
    local failret = false
    if silent then failret = #parts == #arr end
    local path = path_concat(parts, "/")
    local m = (i == #arr and mode or "directory")
    local attr, err = lfs.attributes(path, "mode")
    if attr == nil then
      local ret, err = switch {
        file = function()
          local ret, err = lfs.touch(path)
          if not ret then return false, errfmt(err) end
        end,
        directory = function()
          local ret, err = lfs.mkdir(path)
          if not ret then return false, errfmt(err) end
        end,
      }(m)
      if ret == false then return ret, err end
    elseif attr ~= m then
      if not (silent and #parts == #arr) then
        return false, string.format("'%s' is not a %s", path, m)
      else
        return true
      end
    end
  end
  return true
end

local function create_p(path, ...) create_pf(path_split(path, "/"), ...) end
local function unwrap(ret, err)
  if ret == nil or ret == false then
    glava.fail(err)
  else return ret end
end

function config.Profile:__call(args)
  local self = { name = args.name or ".." }
  self:rebuild()
  return setmetatable(self, config.Profile)
end

function config.Profile:rename(new)
  error("not implemented")
end

function config.Profile:get_path()
  return path_concat(glava.config_path, config.PROFILES_DIR, self.name)
end

function config.Profile:rebuild()
  self.store = {}
  self.path = path_concat(glava.config_path, config.PROFILES_DIR, self.name)
  unwrap(create_p(self.path, "directory", true))
  local unbuilt = {}
  for k, _ in pairs(mappings) do
    unbuilt[k] = true
  end
  for file in lfs.dir(self.path) do
    if file ~= "." and file ~= ".." and mappings[file] ~= nil then
      self:rebuild_file(file, path_concat(path, file))
      unbuilt[file] = nil
    end
  end
  for file, _ in pairs(unbuilt) do
    self:rebuild_file(file, path_concat(path, file), true)
  end
end

function config.Profile:rebuild_file(file, path, phony)
  local fstore = {}
  local fmap = mappings[file]
  self.store[file] = fstore
  
  for k, _ in pairs(fmap) do
    if type(k) == "string" and k ~= "name" then
      unbuilt[k] = true
    end
  end
  
  function parse_line(line, idx, key, default)
    local map = fmap[key]
    if map == nil then return end
    local tt  = type(map.field_type) == "table" and map.field_type or { map.field_type }
    local _,e = string.find(line, MATCH_DATA_PREFIX)
    local at  = string.sub(line, 1, e)
    if default == nil or fstore[key] == nil then
      fstore[key] = {}
    end
    if default == nil then fstore[key].line = idx end
    for t, v in ipairs(tt) do
      local r, i, match = string.find(at, "%s*" .. MATCH_TYPES[v].pattern)
      if r ~= nil then
        -- Handle read-ahead pattern transforms
        if MATCH_TYPES[v].transform ~= nil then
          _, i, match = string.find(at, "%s*" .. MATCH_TYPES[v].transform(match))
        end
        if default == nil or fstore[key][t] == nil then
          fstore[key][t] = MATCH_TYPES[v].cast and MATCH_TYPES[v].cast(match) or match
        end
        at = string.sub(at, 1, i)
      else break end
    end
  end
  
  local idx = 1
  if phony ~= true then
    for line in io.lines(path) do
      local mtype, arg = string.match(line, MATCH_ENTRY_PATTERN)
      if mtype ~= nil then
        parse_line(line, idx, string.format("%s:%s", mtype, arg))
      end
      idx = idx + 1
    end
  end
  
  idx = 1
  for line in io.lines(path_concat(glava.system_shader_path, file)) do
    local mtype, arg = string.match(line, MATCH_ENTRY_PATTERN)
    if mtype ~= nil then
      parse_line(line, idx, string.format("%s:%s", mtype, arg), true)
    end
    idx = idx + 1
  end
end

-- Sync all
function config.Profile:sync()
  for k, v in pairs(self.store) do self:sync_file(k) end
end

-- Sync filename relative to profile root
function config.Profile:sync_file(fname)
  local fstore = self.store[fname]
  local fmap   = mappings[file]
  local fpath  = path_concat(self.path, fname)
  local buf    = {}
  local extra  = {}
  local idx    = 1
  for k, v in fstore do
    local parts = { string.format("#%s", string.gsub(k, ":", " ")) }
    local field = fmap[k].field_type
    for i, e in ipairs(type(field) == "table" and field or { field }) do
      parts[#parts + 1] = MATCH_TYPES[e].serialize and MATCH_TYPES[e].serialize(v[i]) or v[i]
    end
    local serialized = table.concat(parts, " ")
    if v.line then buf[line] = serialized
    else extra[#extra + 1] = serialized end
  end
  if lfs.attributes(fpath, "mode") == "file" then
    for line in io.lines(path) do
      if not buf[idx] then
        buf[idx] = line
      end
      idx = idx + 1
    end
    for _, v in ipairs(extra) do
      buf[#buf + 1] = v
    end
  end
  local handle, err = io.open(fpath, "w+")
  if handle then
    handle:write(table.concat(buf, "\n"))
    handle:close()
  else
    glava.fail(string.format("Could not open file handle to \"%s\": %s", handle, err))
  end
end

return config
