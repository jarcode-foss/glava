
local radius = 128
local d = radius * 2
d = math.floor(math.sqrt((d * d) / 2))

local really_long_name = "Extremely Lengthly Song Name"
local header_lbl = ui.label { contents = really_long_name, font = "h2" }
local header_lbl2 = ui.label { contents = really_long_name, font = "h2",
                               color = { r = 0.2, g = 0.2, b = 0.2}}
local header = ui.layer.scroll(d, header_lbl)
local header2 = ui.layer.scroll(d, header_lbl2)
local lower  = ui.label { contents = "Carpenter Brut" }

ui.root = ui.layer.as_component(d, d, ui.free_container { header2, header, lower })
ui.root:color({ a = 0.45 })

function on_resize()
    local px = (ui.width / 2) - (d / 2)
    local py = (ui.height / 2) - (d / 2)
    ui.root:position(px, py)
end

function position_labels()
    local top = (d / 2) + 6
    local hpos = (d - header_lbl.width) / 2
    local lpos = (d - lower.width) / 2
    if hpos < 0 then hpos = 0 end
    if lpos < 0 then lpos = 0 end
    header:baseline(hpos, top)
    header2:baseline(hpos - 1, top - 1)
    lower:baseline(hpos > lpos and lpos or hpos, top - lower.face.baseline)
end

position_labels()
