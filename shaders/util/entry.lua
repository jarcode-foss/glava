
-- `root_path` is defined in the global table in C code, and contains
-- the path of the root configuration directory. `shader_mod` is the
-- name of the currently selected GLSL module.
package.path = package.path
    .. ";" .. root_path .. "/?/init.lua"
    .. ";" .. root_path .. "/?.lua"
    .. ";" .. root_path .. "/util/?.lua"

-- A component must/can contain:
-- * (optional) `render()`                 contains drawing logic with allotted space
-- * (optional) `render_fonts()`           contains font drawing logic with allotted space
-- * `rebuild(child_has_rebuilt)`          called when sizes change
-- * (optional) `needs_rebuild`            checked if the component wants to rebuild
-- * (optional) `children`                 called for children iterator

-- called when the OpenGL context is properly initialized
function setup(mod)
    ui.faces = {
        ["h1"] = ui.font("/usr/share/fonts/TTF/Roboto-Regular.ttf", 24),
        ["h2"] = ui.font("/usr/share/fonts/TTF/Roboto-Bold.ttf", 16),
        ["normal"] = ui.font("/usr/share/fonts/TTF/Roboto-Light.ttf", 14),
        ["mono"] = ui.font("/usr/share/fonts/TTF/DejaVuSansMono.ttf", 14)
    }
    ui.faces["normal"]:select()
    
    package.path = root_path .. "/" .. mod  .. "/?/init.lua"
        .. ";" .. root_path .. "/" .. mod  .. "/?.lua;" .. package.path
    
    require("ui")
    
    print "Loaded 'ui' module"
    if ui.root == nil then
        print "WARNING: selected module does not assign a component to `ui.root`!"
    end
end

function ui.impl_defaults(c)
    -- default implementations
    function c:children() return pairs({}) end
    function c:render()
        for _, v in self:children() do
            if v.render then v:render() end
        end
    end
    function c:render_fonts()
        for _, v in self:children() do
            if v.render_fonts then v:render_fonts() end
        end
    end
    function c:rebuild(child_has_rebuilt) end
end

function ui.new_component(constructor)
    local c = { mt = {} }
    c.__index = c
    setmetatable(c, c.mt)
    function c.mt:__call(...)
        local self = setmetatable({}, c)
        constructor(self, ...)
        return self
    end
    ui.impl_defaults(c)
    return c
end

local layer_data = setmetatable({}, { __mode = "k" })
ui.impl_defaults(ui.layer)

function ui.layer:__index(key)
    return ui.layer[key] or layer_data[self][key]
end

function ui.layer:__newindex(key, value)
    layer_data[self][key] = value
end

function ui.layer.as_component(w, h, component)
    local self = ui.layer(w, h)
    layer_data[self] = {}
    self:handlers(
        function()
            if self.child.render then
                self.child:render() end end,
        function()
            if self.child.render_fonts then
                self.child:render_fonts() end end)
    self.child = component
    component.parent = self
    return self
end

local pad = 20
function ui.layer.scroll(w, component)
    local cw, ch = component:area()
    component:position(0, 0)
    local self = ui.layer.as_component(w, ch, component)
    if cw > w then
        self:resize(w, ch, cw + pad, ch)
        self.needs_rebuild = true
        self.always_rebuild = true
    end
    return self
end

-- Only works if the child is a label
function ui.layer:baseline(x, y)
    self:position(x, y - self.child.face.descender)
end
function ui.layer:rebuild(h) if h then self:draw_contents() end end
function ui.layer:children() return pairs({ self.child }) end
function ui.layer:render() self:draw() end
function ui.layer:render_fonts() end

function ui.iter_flags_back(comp, parent, ...)
    comp.needs_rebuild = true
    if parent then ui.iter_flags_back(parent, ...) end
end

function ui.iter_flags(comp, ...)
    for _, v in comp:children() do
        -- if this component needs to be rebuilt, we need to iterate backwards and flag its parents
        if v.needs_rebuild then ui.iter_flags_back(comp, ...) end
        -- iterate children
        ui.iter_flags(v, comp, ...)
    end
end

function ui.iter_rebuild(comp)
    -- rebuild children first
    local child_has_rebuilt = false
    for _, v in comp:children() do
        if ui.iter_rebuild(v) then child_has_rebuilt = true end
    end
    -- rebuild elements in-place
    if comp.needs_rebuild then
        if not comp.always_rebuild then comp.needs_rebuild = false end
        comp:rebuild(child_has_rebuilt)
        return true
    end
    return child_has_rebuilt
end

local function flag_all(comp)
    for _, v in comp:children() do flag_all(v) end
    comp.needs_rebuild = true
end

function draw(w, h)
    if not ui.root then return end
    
    local notify_resize = false
    if ui.width ~= w or ui.height ~=h then
        notify_resize = true
    end
    
    ui.width  = w
    ui.height = h
    
    if notify_resize then
        if on_resize then on_resize() end
        flag_all(ui.root)
    end
    
    ui.iter_flags(ui.root)
    ui.iter_rebuild(ui.root)
    
    if ui.root then
        ui.root:render()
    end
end

-- Single-line text label
ui.label = ui.new_component(
    function(self, args)
        self.handle = ui.text()
        self:font(args.font or "normal")
        if args.contents then self:contents(args.contents) end
        if args.color and type(args.color) == "table" then
            self.color = args.color
        else
            self.color = { r = 0.8, g = 0.8, b = 0.8, a = 1.0 }
        end
end)

function ui.label:baseline(x, y)
    self.handle:position(x, y)
    self.needs_rebuild = true
end

function ui.label:position(x, y)
    self.handle:position(x, y + self.face.descender)
    self.needs_rebuild = true
end

function ui.label:area()
    return self.width, self.face.baseline
end

function ui.label:font(font)
    if font then
        self.font_key = font
        self.face = ui.faces[font]
        self:calc_width()
        self.needs_rebuild = true
    end
    return self.contents
end

function ui.label:contents(str)
    if str then
        self.str = str
        self:calc_width()
        self.needs_rebuild = true
    end
    return self.str
end

function ui.label:calc_width()
    if self.str then
        self.face:select()
        local w = 0
        for _, v in utf8.pairs(self.str) do
            w = w + ui.advance(utf8.codepoint(v))
        end
        self.width = w
    end
end

function ui.label:color(r, g, b, a)
    if r then self.color.r = r end
    if g then self.color.g = g end
    if b then self.color.b = b end
    if a then self.color.a = a end
    self.needs_rebuild = true
end

function ui.label:rebuild(_)
    self.face:select()
    self.handle:contents(self.str, self.color)
end

function ui.label:render_fonts()
    self.handle:draw()
end

-- Free container

ui.free_container = ui.new_component(
    function(self, args)
        self.contents = args
end)

function ui.free_container:children()
    return pairs(self.contents)
end

local request_handlers = {}
requests = {}

function request(name, ...)
    local f = request_handlers[name]
    if f then f(...) else
        requests[#requests + 1] = { name, ... }
    end
end
