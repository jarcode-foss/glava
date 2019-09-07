local lgi = require "lgi"
local Gdk = lgi.Gdk

local utils = {}

function utils.infer_color_bits(x)  
  if x:sub(1, 1) ~= "#" then
    x = "#" .. x
  end
  for i = 1, 9 - x:len() do
    x = x .. (x:len() >= 7 and "F" or "0")
  end
  return x
end

function utils.sanitize_color(x)
  return utils.infer_color_bits(x):sub(1, 9):gsub("[^#0-9a-fA-F]", "0")
end

function utils.parse_color_rgba(x)
  local x = utils.infer_color_bits(x)
  return Gdk.RGBA.parse(
    string.format(
      "rgba(%d,%d,%d,%f)",
      tonumber(x:sub(2, 3), 16),
      tonumber(x:sub(4, 5), 16),
      tonumber(x:sub(6, 7), 16),
      tonumber(x:sub(8, 9), 16) / 255
    )
  )
end

function utils.rgba_to_gdk_color(x)
  return Gdk.Color(
    math.floor(x.red   * 255 + 0.5),
    math.floor(x.green * 255 + 0.5),
    math.floor(x.blue  * 255 + 0.5)
  )
end

function utils.rgba_to_integral(x)
  return {
    red   = math.floor(x.red   * 255 + 0.5),
    green = math.floor(x.green * 255 + 0.5),
    blue  = math.floor(x.blue  * 255 + 0.5)
  }
end

function utils.format_color_rgba(x)
  return string.format(
    "#%02X%02X%02X%02X",
    math.floor(x.red   * 255 + 0.5),
    math.floor(x.green * 255 + 0.5),
    math.floor(x.blue  * 255 + 0.5),
    math.floor(x.alpha * 255 + 0.5)
  )
end

function utils.format_color_rgb(x)
  return string.format(
    "#%02X%02X%02X",
    math.floor(x.red   * 255 + 0.5),
    math.floor(x.green * 255 + 0.5),
    math.floor(x.blue  * 255 + 0.5)
  )
end

return utils
