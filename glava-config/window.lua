--[[
  MAINTAINER NOTICE:
  
  This application aims to be both Gtk+ 3 and 4 compatible for future-proofing. This means
  avoiding *every* deprecated widget in Gtk+ 3, and watching out for some old functionality:
  
  * Gdk.Color usage, use Gdk.RGBA instead
  * Pango styles and style overrides
  * Check convenience wrappers for deprecation, ie. GtkColorButton
  * Avoid seldom used containers, as they may have been removed in 4.x (ie. GtkButtonBox)
  
  In some cases we use deprecated widgets or 3.x restricted functionality, but only when we
  query that the types are available from LGI (and otherwise use 4.x compatible code).
]]

return function()
  local lgi       = require 'lgi'
  local utils     = require 'glava-config.utils'
  local mappings  = require 'glava-config.mappings'
  local GObject   = lgi.GObject
  local Gtk       = lgi.Gtk
  local Pango     = lgi.Pango
  local Gdk       = lgi.Gdk
  local GdkPixbuf = lgi.GdkPixbuf
  local cairo     = lgi.cairo
  
  -- Both `GtkColorChooserDialog` and `GtkColorSelectionDialog` are
  -- supported by this tool, but the latter is deprecated and does
  -- not exist in 4.x releases.
  --
  -- The old chooser, however, is objectively better so let's try
  -- to use it if it exists.
  local use_old_chooser = true
  if Gtk.get_major_version() >= 4 then
    use_old_chooser = false
  end
  
  local window
  
  local repeat_pattern = cairo.SurfacePattern(
    cairo.ImageSurface.create_from_png(glava.resource_path .. "transparent.png")
  )
  repeat_pattern:set_extend("REPEAT")
  
  -- We need to define a CSS class to use an alternative font for
  -- color and identity entries; used to indicate to the user that
  -- the field has formatting requirements
  local cssp = Gtk.CssProvider {}
  cssp:load_from_data(".fixed-width-font-entry { font-family: \"Monospace\"; }")
  
  local ItemColumn = {
    PROFILE   = 1,
    ENABLED   = 2,
    ACTIVABLE = 3,
    WEIGHT    = 4,
    VISIBLE   = 5
  }
  
  -- Fill store with initial items.
  local item_store = Gtk.ListStore.new {
    [ItemColumn.PROFILE]   = GObject.Type.STRING,
    [ItemColumn.ENABLED]   = GObject.Type.BOOLEAN,
    [ItemColumn.ACTIVABLE] = GObject.Type.BOOLEAN,
    [ItemColumn.VISIBLE]   = GObject.Type.BOOLEAN,
    [ItemColumn.WEIGHT]    = GObject.Type.INT
  }

  local default_entry = {
    [ItemColumn.PROFILE]   = "Default",
    [ItemColumn.ENABLED]   = false,
    [ItemColumn.VISIBLE]   = false,
    [ItemColumn.ACTIVABLE] = false,
    [ItemColumn.WEIGHT]    = 600
  }

  -- Apply `t[k] = v` to all table argument at array indexes,
  -- and return the unpacked list of tables. Used for nesting
  -- widget construction.
  local function apply(tbl)
    local ret = {}
    for k, v in ipairs(tbl) do
      ret[k] = v
      tbl[k] = nil
    end
    for k, v in pairs(tbl) do
      for _, r in ipairs(ret) do
        r[k] = v
      end
    end
    return unpack(ret)
  end
  
  -- Apply `binds[k] = v` while returning unpacked values
  local binds = {}
  local function bind(tbl)
    local ret = {}
    for k, v in pairs(tbl) do
      binds[k] = v
      ret[#ret + 1] = v
    end
    return unpack(ret)
  end
  
  local function link(tbl)
    for _, v in ipairs(tbl) do
      v:get_style_context():add_class("linked")
    end
    return unpack(tbl)
  end
  
  local function ComboBoxFixed(tbl)
    local inst = Gtk.ComboBoxText { id = tbl.id }
    for _, v in pairs(tbl) do
        inst:append_text(v)
    end
    inst:set_active(tbl.default or 0)
    return inst
  end
  
  local SpoilerView = function(tbl)
    local stack = Gtk.Stack {
      expand = true,
      transition_type = Gtk.StackTransitionType.CROSSFADE
    }
    local btn = Gtk.CheckButton {
      active = tbl.active or false
    }
    if tbl.active ~= true then
      stack:add_named(Gtk.Box {}, "none")
    end
    stack:add_named(tbl[1], "view")
    if tbl.active == true then
      stack:add_named(Gtk.Box {}, "none")
    end
    function btn:on_toggled(path)
      stack:set_visible_child_name(btn.active and "view" or "none")
    end
    return Gtk.Box {
      expand      = false,
      orientation = "VERTICAL",
      spacing     = 4,
      Gtk.Box {
        orientation = "HORIZONTAL",
        spacing     = 6,
        btn,
        Gtk.Label { label = tbl.label or "Spoiler" }
      },
      Gtk.Separator(),
      stack
    }
  end
  
  local ConfigView = function(tbl)
    local grid = {
      row_spacing        = 2,
      column_spacing     = 12,
      column_homogeneous = false,
      row_homogeneous    = false
    }
    local list = {}
    local idx  = 0
    local function cbuild(list, entry)
      list[#list + 1] = {
        Gtk.Label { label = entry[1], halign = "START", valign = "START" },
        left_attach = 0, top_attach = idx
      }
      list[#list + 1] = {
        Gtk.Box { hexpand = true },
        left_attach = 1, top_attach = idx
      }
      list[#list + 1] = {
        apply { halign = "END", entry[3] or Gtk.Box {} },
        left_attach = 2, top_attach = idx
      }
      list[#list + 1] = {
        apply { halign = "FILL", hexpand = false, entry[2] },
        left_attach = 3, top_attach = idx
      }
      list[#list + 1] = {
        Gtk.Separator {
          vexpand = false
        }, left_attach = 0, top_attach = idx + 1, width = 3
      }
      idx = idx + 2
    end
    for _, entry in ipairs(tbl) do
      cbuild(list, entry)
    end
    local adv = {}
    if tbl.advanced then
      idx = 0
      for _, entry in ipairs(tbl.advanced) do
        cbuild(adv, entry)
      end
    end
    for k, v in pairs(grid) do
      list[k] = v
      adv[k] = v
    end
    return Gtk.ScrolledWindow {
      expand = true,
      Gtk.Box {
        margin_top   = 12,
        margin_start = 16,
        margin_end   = 16,
        hexpand      = true,
        vexpand      = true,
        halign       = "FILL",
        orientation  = "VERTICAL",
        spacing      = 6,
        Gtk.Grid(list),
        #adv > 0 and SpoilerView
        { label = "Show Advanced",
          Gtk.Grid(adv)
        } or Gtk.Box {}
    } }
  end
  local function wrap_label(widget, label)
    if label then
      widget = Gtk.Box {
        orientation = "HORIZONTAL",
        spacing     = 6,
        Gtk.Label {
          label = label
        }, widget
      }
    end
    return widget
  end
  
  -- Generators for producing widgets (and their layouts) that bind to configuration values
  -- note: `get_data` returns stringified data
  local widget_generators
  widget_generators = {
    -- A switch to represent a true/false value
    ["boolean"] = function(attrs)
      local widget = Gtk.Switch { hexpand = false }
      return {
        widget = Gtk.Box { Gtk.Box { hexpand = true }, wrap_label(widget, attrs.label) },
        set_data = function(x)
          widget.active = x
          return true
        end,
        get_data = function() return widget.active end,
        connect = function(f) widget.on_state_set = f end
      }
    end,
    -- Entry for a generic string, may have predefined selections
    ["string"] = function(attrs)
      local widget = apply {
        attrs.entries ~= nil
          and apply { ComboBoxFixed(attrs.entries) }
          or Gtk.Entry { width_chars = 12 },
        hexpand = true
      }
      return {
        widget   = wrap_label(widget, attrs.label),
        internal = widget,
        set_data = function(x)
          if not attrs.entries then
            widget:set_text(x)
          else
            for k, v in ipairs(attrs.entries) do
              if v == x then
                widget:set_active(v - 1)
                return true
              end
            end
            return false
          end
          return true
        end,
        get_data = function()
          local text = (not attrs.entries) and widget:get_text() or widget:get_active_text()
          if attrs.translate then
            text = attrs.translate[text]
          end
          return text
        end,
        connect = function(f)
          -- Note: the underlying widget can be `GtkComboBoxText` or `GtkEntry`;
          -- they simply just use the same signal for user input
          widget.on_changed = f
        end
      }
    end,
    -- Entry for a valid C/GLSL identity, may have predefined selections
    ["ident"] = function(attrs)
      local s = widget_generators.string(attrs)
      -- Set fixed-width font if the users enter/select identifiers by their name,
      -- rather than a description to indicate it's a GLSL identity
      if not attrs.translate then
        s.internal:get_style_context():add_provider(cssp, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        s.internal:get_style_context():add_class("fixed-width-font-entry")
      end
      if not attrs.entries and not attrs._ignore_restrict then
        -- Handle idenifier formatting for entries without a preset list
        local handlers = {}
        local function run_handlers()
          for _, f in ipairs(handlers) do f() end
        end
        function s.internal:on_changed()
          local i = s.internal.text
          if i:match("[^%w]") ~= nil or i:sub(1, 1):match("[^%a]") ~= nil then
            s.internal.text = i:gsub("[^%w]", ""):gsub("^[^%a]+", "")
          else
            run_handlers()
          end
        end
        s.connect = function(f)
          handlers[#handlers + 1] = f
        end
      end
      return s
    end,
    -- A full GLSL expression
    ["expr"] = function(attrs)
      -- Expressions can be implemented by using the identity field and disabling
      -- input format restrictions.
      attrs._ignore_restrict = true
      return widget_generators.ident(attrs)
    end,
    -- Adjustable and bound floating-point value
    ["float"] = function(attrs)
      local widget = Gtk.SpinButton {
        hexpand = true,
        adjustment = Gtk.Adjustment {
          lower          = attrs.lower or 0,
          upper          = attrs.upper or 100,
          page_size      = 1,
          step_increment = attrs.increment or 1,
          page_increment = attrs.increment or 1
        },
        width_chars = attrs.width or 6,
        numeric     = true,
        digits      = attrs.digits or 2, 
        climb_rate  = attrs.increment or 1
      }
      return {
        widget = wrap_label(widget, attrs.label),
        set_data = function(x)
          widget:set_text(x)
          return true
        end,
        get_data = function() return widget:get_text() end,
        connect = function(f) widget.on_value_changed = f end
      }
    end,
    -- Adjustable and bound integral value
    ["int"] = function(attrs)
      local widget = Gtk.SpinButton {
        hexpand = true,
        adjustment = Gtk.Adjustment {
          lower          = attrs.lower or 0,
          upper          = attrs.upper or 100,
          page_size      = 1,
          step_increment = attrs.increment or 1,
          page_increment = attrs.increment or 1
        },
        width_chars = attrs.width or 6,
        numeric     = true,
        digits      = 0, 
        climb_rate  = attrs.increment or 1
      }
      return {
        widget = wrap_label(apply { vexpand = false, widget }, attrs.label),
        set_data = function(x)
          widget:set_text(x)
          return true
        end,
        get_data = function() return widget:get_text() end,
        connect = function(f) widget.on_value_changed = f end
      }
    end,
    -- The color type is the hardest to implement; as Gtk deprecated
    -- the old color chooser button, so we have to implement our own.
    -- The benefits of doing this mean we get to use the "nice" Gtk3
    -- chooser, and the button rendering itself is much better.
    ["color"] = function(attrs)
      local dialog_open = false
      local handlers = {}
      local function run_handlers()
          for _, f in ipairs(handlers) do f() end
      end
      local c = Gdk.RGBA {
        red = 1.0, green = 1.0, blue = 1.0, alpha = 1.0
      }
      local area = Gtk.DrawingArea()
      area:set_size_request(16, 16)
      local draw = function(widget, cr)
        local context = widget:get_style_context()
        local width   = widget:get_allocated_width()
        local height  = widget:get_allocated_height()
        local aargc   = { width / 2, height / 2, math.min(width, height) / 2, 0, 2 * math.pi }
        Gtk.render_background(context, cr, 0, 0, width, height)
        cr:set_source(repeat_pattern)
        cr:arc(unpack(aargc))
        cr:fill()
        cr:set_source_rgba(c.red, c.green, c.blue, c.alpha)
        cr:arc(unpack(aargc))
        cr:fill()
      end
      if Gtk.get_major_version() >= 4 then
        area:set_draw_func(draw)
      else
        area.on_draw = draw
      end
      local btn = Gtk.Button {
        apply {
          margin_top    = 1,
          margin_bottom = 1,
          area
      } }
      local entry = Gtk.Entry {
        hexpand     = true, 
        width_chars = 9,
        max_length  = 9,
        text        = attrs.alpha and "#FFFFFFFF" or "#FFFFFF"
      }
      entry:get_style_context():add_provider(cssp, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
      entry:get_style_context():add_class("fixed-width-font-entry")
      local widget = Gtk.Box {
        orientation = "HORIZONTAL",
        spacing     = 0,
        entry, btn
      }
      link { widget }
      widget = wrap_label(widget, attrs.label)
      function btn:on_clicked()
        local c_change_staged = false
        local dialog = (use_old_chooser and Gtk.ColorSelectionDialog or Gtk.ColorChooserDialog)
        { title               = "Select Color",
          transient_for       = window,
          modal               = true,
          destroy_with_parent = true
        }
        if use_old_chooser then
          dialog.cancel_button:set_visible(false)
          dialog.ok_button.label = "Close"
          dialog.color_selection.current_rgba = c
          if attrs.alpha then
            dialog.color_selection.has_opacity_control = true
          end
          function dialog.color_selection:on_color_changed()
            c_change_staged = true
            c = dialog.color_selection.current_rgba
            entry:set_text(attrs.alpha and utils.format_color_rgba(c) or utils.format_color_rgb(c))
            area:queue_draw()
          end
        else
          dialog.rgba = c
          if attrs.alpha then
            dialog.use_alpha = true
          end
        end

        dialog_open = true
        local ret = dialog:run()
        dialog_open = false
        dialog:set_visible(false)
        
        if not use_old_chooser and ret == Gtk.ResponseType.OK then
          c = dialog.rgba
          entry:set_text(attrs.alpha and utils.format_color_rgba(c) or utils.format_color_rgb(c))
          area:queue_draw()
          run_handlers()
        elseif use_old_chooser and c_change_staged then
          run_handlers()
        end
      end
      function entry:on_changed()
        local s = utils.sanitize_color(entry.text)
        c = utils.parse_color_rgba(s)
        area:queue_draw()
        if not dialog_open then run_handlers() end
      end
      return {
        widget = widget,
        set_data = function(x)
          local s = utils.sanitize_color(x)
          c = utils.parse_color_rgba(s)
          area:queue_draw()
          entry:set_text(s)
          return true
        end,
        get_data = function(x)
          return attrs.alpha and utils.format_color_rgba(c) or utils.format_color_rgb(c)
        end,
        connect = function(f)
          handlers[#handlers + 1] = f
        end
      }
    end,
    -- A field capable of producing a GLSL color expression.
    ["color-expr"] = function(attrs, header)
      -- Define color control variables for use in color expressions
      local controls = {
        { "Baseline", "d" },
        { "X axis", "gl_FragCoord.x" },
        { "Y axis", "gl_FragCoord.y" }
      }
      local control_list = {}
      for i, v in ipairs(controls) do
        control_list[i] = v[1]
        controls[v[1]] = v[2]
      end
      
      -- Define color expression types. Field data is assigned according
      -- to the associated pattern, and entries are ordered in terms of
      -- match priority
      local cetypes = {
        { "Gradient",
          fields = {
            { "color" },
            { "color" },
            { "ident",
              entries   = control_list,
              translate = controls,
              header    = "Axis:"
            },
            { "float",
              upper  = 1000,
              lower  = -1000,
              header = "Scale:"
          } },
          -- match against GLSL mix expression, ie.
          -- `mix(#3366b2, #a0a0b2, clamp(d / GRADIENT, 0, 1))`
          match = "mix%s*%(" ..
            "%s*(#[%dA-Fa-f]*)%s*," ..
            "%s*(#[%dA-Fa-f]*)%s*," ..
            "%s*clamp%s*%(%s*(%w+)%s*/%s*(%w+)%s*,%s*0%s*,%s*1%s*%)%s*%)",
          output = "mix(%s, %s, clamp(%s / %s, 0, 1))"
        },
        { "Solid",
          fields = { { "color" } },
          match = "#[%dA-Fa-f]*",
          output = "%s",
          default = true
      } }
      
      local stack  = Gtk.Stack { vhomogeneous = false }
      local hstack = Gtk.Stack { vhomogeneous = false }
      
      local cekeys  = {}
      local default = nil
      for i, v in ipairs(cetypes) do
        if not v.default then
          cekeys[#cekeys + 1] = v[1]
        else
          table.insert(cekeys, 1, v[1])
        end
        cetypes[v[1]] = v
        local wfields = {}
        local hfields = {
          Gtk.Label {
            halign = "END",
            valign = "START",
            label  = header
        } }
        local gen = {}
        for k, e in ipairs(v.fields) do
          v.alpha = attrs.alpha
          local g = widget_generators[e[1]](e)
          gen[#gen + 1] = g
          wfields[k] = g.widget
          hfields[#hfields + 1] = Gtk.Label {
            halign = "END",
            label  = e.header
          }
        end
        v.gen = gen
        v.widget = Gtk.Box(
          apply {
            homogeneous = true,
            orientation = "VERTICAL",
            spacing     = 1,
            wfields
        } )
        v.hwidget = Gtk.Box(
          apply {
            homogeneous = true,
            orientation = "VERTICAL",
            spacing     = 1,
            hfields
        } )
        hstack:add_named(v.hwidget, v[1]) 
        stack:add_named(v.widget, v[1])
        if v.default then
          default = v[1]
        end
        v.set_data = function(x)
          for i, m in ipairs { string.match(x, v.match) } do
            gen[i].set_data(m)
          end
        end
        v.get_data = function()
          local fields = {}
          for i = 1, #v.fields do
            fields[i] = gen[i]:get_data()
          end
          return string.format(v.output, unpack(fields))
        end
        v.connect = function(f)
          for _, g in ipairs(gen) do
            g.connect(f)
          end
        end
      end
      local cbox = apply {
        hexpand = true,
        ComboBoxFixed(cekeys)
      }
      stack:set_visible_child(cetypes[default].widget)
      hstack:set_visible_child(cetypes[default].hwidget)
      cetypes[default].widget:show()
      cetypes[default].hwidget:show()
      function cbox:on_changed()
        local t = cbox:get_active_text()
        stack:set_visible_child_name(t)
        hstack:set_visible_child_name(t)
      end
      local widget = Gtk.Box {
        orientation = "VERTICAL",
        spacing     = 1,
        wrap_label(cbox, attrs.label), stack
      }
      return {
        widget        = widget,
        header_widget = hstack,
        set_data = function(x)
          for i, v in ipairs(cetypes) do
            if string.match(x, v.match) ~= nil then
              v.set_data(x)
              return true
            end
          end
          return false
        end,
        get_data = function()
          return cetypes[cbox:get_active_text()].get_data()
        end,
        connect = function(f)
          for i, v in ipairs(cetypes) do
            v.connect(f)
          end
        end
      }
    end
  }
  
  -- Extra widget for special service/autostart functionality
  local ServiceView = function(self)
    local switch = Gtk.Switch {
      sensitive  = false,
      hexpand    = false
    }
    local method = ComboBoxFixed {
      "None",
      "SystemD User Service",
      "InitD Entry",
      "Desktop Entry"
    }
    method.on_changed = function(box)
      local opt = box:get_active_text()
      switch.sensitive = opt ~= "None"
      if switch.active == true and opt == "None" then
        switch:activate()
      end
      for _, entry in item_store:pairs() do
        if entry[ItemColumn.PROFILE] == self.name then
          entry[ItemColumn.ACTIVABLE] = opt ~= "None"
          if opt == "None" then
            entry[ItemColumn.ENABLED] = false
          end
        end
      end
    end
    switch.on_notify["active"] = function(inst, pspec)
      for _, entry in item_store:pairs() do
        if entry[ItemColumn.PROFILE] == self.name then
          entry[ItemColumn.ENABLED] = switch.active
        end
      end
      -- TODO handle enable here
    end
    return ConfigView {
      { "Enabled", Gtk.Box { Gtk.Box { hexpand = true }, switch } },
      { "Autostart Method", method }
    }, switch
  end
  
  -- Produce a widget containing a scroll area full of widgets bound to
  -- requests/defines in the specified profile.
  local function ProfileView(name)
    local self = { name = name }
    local args = {}
    for k, v in pairs(mappings) do
      local layout = {}
      for _, e in ipairs(v) do
        if type(e) == "table" then
          local header = nil
          local fields = {}
          local ftypes = type(e.field_type) == "table" and e.field_type or { e.field_type }
          local fattrs = type(e.field_type) == "table" and e.field_attrs or { e.field_attrs }
          if not fattrs then fattrs = {} end
          for i, f in ipairs(ftypes) do
            local entry = widget_generators[f](fattrs[i] or {}, e.header)
            if not header then
              header = entry.header_widget
            end
            fields[#fields + 1] = entry.widget
            -- todo: finish linking config
            entry.connect(function()
                print(string.format("assign %s->%s->%s[%d] = %s", k, e[1], f, i, tostring(entry.get_data())))
            end)
          end
          -- disable header display widget if there are multiple fields
          if #fields > 1 then header = nil end
          fields.orientation = "VERTICAL"
          fields.spacing = 2
          local fwidget = {
            e.description,
            #fields > 1 and
              Gtk.Frame {
                label = fattrs.frame_label,
                apply {
                  margin_start  = 4,
                  margin_end    = 4,
                  margin_top    = 4,
                  margin_bottom = 4,
                  Gtk.Box(fields)
              } } or fields[1],
            header or (e.header and Gtk.Label { valign = "START", label = e.header } or Gtk.Box {})
          }
          if not e.advanced then
            layout[#layout + 1] = fwidget
          else
            if not layout.advanced then layout.advanced = {} end
            layout.advanced[#layout.advanced + 1] = fwidget
          end
        end
      end
      args[#args + 1] = { tab_label = v.name, ConfigView(layout) }
    end
    local service, chk = ServiceView(self)
    args[#args + 1] = {
      tab_label = "Autostart",
      name ~= "Default" and service or
        Gtk.Box {
          valign      = "CENTER",
          orientation = "VERTICAL",
          spacing     = 8,
          Gtk.Label {
            label = "Autostart options are not available for the default user profile."
          },
          Gtk.Button {
            hexpand = false,
            halign  = "CENTER",
            label   = "Show Profiles"
    } } }
    args.expand = true
    notebook = Gtk.Notebook(args)
    notebook:show_all()
    self.widget = notebook
    self.autostart_enabled = chk
    function self:rename(new)
      self.name = new
    end
    function self:delete()
      
    end
    return self;
  end
  
  local view_registry = {}
  view_registry[default_entry[ItemColumn.PROFILE]] = ProfileView(default_entry[ItemColumn.PROFILE])
  item_store:append(default_entry)
  
  window = Gtk.Window {
    title = "GLava Config",
    default_width = 320,
    default_height = 200,
    border_width = 5,
    Gtk.Box {
      orientation = "HORIZONTAL",
      spacing = 6,
      homogeneous = false,
      Gtk.Box {
        hexpand = false,
        orientation = "VERTICAL",
        spacing = 5,
        Gtk.ScrolledWindow {
          shadow_type = "ETCHED_IN",
          vexpand = true,
          width_request = 200,
          bind {
            view = Gtk.TreeView {
              model = item_store,
              activate_on_single_click = true,
              Gtk.TreeViewColumn {
                title  = "Profile",
                expand = true,
                { bind { profile_renderer = Gtk.CellRendererText {} },
                  { text     = ItemColumn.PROFILE,
                    editable = ItemColumn.VISIBLE,
                    weight   = ItemColumn.WEIGHT
              } } },
              Gtk.TreeViewColumn {
                title     = "Enabled",
                alignment = 0.5,
                -- Note `xalign` usage here comes from GtkCellRenderer, which unlike the
                -- legacy alignment widget is not deprecated
                { bind { toggle_renderer = Gtk.CellRendererToggle { xalign = 0.5 } },
                  { active      = ItemColumn.ENABLED,
                    activatable = ItemColumn.ACTIVABLE,
                    visible     = ItemColumn.VISIBLE
        } } } } } },
        link {
          Gtk.Box {
            hexpand = true,
            bind {
              reload = Gtk.Button {
                Gtk.Image {
                  icon_name = "view-refresh-symbolic"
              } },
            },
            bind {
              add = Gtk.Button {
                halign  = "FILL",
                hexpand = true,
                label   = "Create Profile",
            } },
            bind {
              remove = Gtk.Button {
                halign    = "END",
                sensitive = false,
                Gtk.Image {
                  icon_name = "user-trash-symbolic"
      } } } } } },
      Gtk.Box {
        orientation = "VERTICAL",
        spacing     = 6,
        link {
          Gtk.Box {
            Gtk.ToggleButton {
              Gtk.Image {
                icon_name = "view-paged-symbolic"
              },
              on_clicked = function()
                --
              end
            },
            bind {
              display_path = Gtk.Entry {
              -- todo: bind to config 
              text = "~/.config/glava/rc.glsl",
              editable = false,
              hexpand = true
        } } } },
        bind {
          stack_view = Gtk.Stack {
            expand = true,
            transition_type = Gtk.StackTransitionType.CROSSFADE
  } } } } }
  
  local selection = binds.view:get_selection()
  selection.mode = 'SINGLE'
  binds.stack_view:add_named(view_registry[default_entry[ItemColumn.PROFILE]].widget,
                                    default_entry[ItemColumn.PROFILE])
  
  function unique_profile(profile_name_proto)
    local profile_idx  = 0
    local profile_name = profile_name_proto
    while true do
      local used = false
      for i, entry in item_store:pairs() do
        if entry[ItemColumn.PROFILE] == profile_name then
          used = true
        end
      end
      if not used then break else
        profile_idx  = profile_idx + 1
        profile_name = profile_name_proto .. " (" .. tostring(profile_idx) .. ")"
      end
    end
    return profile_name
  end
  
  function binds.view:on_row_activated(path, column)
    local name = item_store[path][ItemColumn.PROFILE]
    binds.stack_view:set_visible_child_name(name)
    binds.remove.sensitive = (name ~= "Default")
  end
  
  function binds.profile_renderer:on_edited(path_string, new_profile)
    local path = Gtk.TreePath.new_from_string(path_string)
    local old = item_store[path][ItemColumn.PROFILE]
    local store = binds.stack_view:get_child_by_name(old)
    new_profile = string.match(new_profile, "^%s*(.-)%s*$")
    if old == new_profile or new_profile == "Default" then return end
    new_profile = unique_profile(new_profile)
    print("Renamining profile \"" .. old .. "\" -> \"" .. new_profile .. "\"")
    binds.stack_view:remove(store)
    binds.stack_view:add_named(store, new_profile)
    local vstore = view_registry[old]
    view_registry[old] = nil
    view_registry[new_profile] = vstore
    vstore:rename(new_profile)
    item_store[path][ItemColumn.PROFILE] = new_profile
  end
  
  function binds.toggle_renderer:on_toggled(path_string)
    local path = Gtk.TreePath.new_from_string(path_string)
    if view_registry[item_store[path][ItemColumn.PROFILE]].autostart_enabled.active
    ~= not item_store[path][ItemColumn.ENABLED] then
      view_registry[item_store[path][ItemColumn.PROFILE]].autostart_enabled:activate()
    end
    item_store[path][ItemColumn.ENABLED] =
      view_registry[item_store[path][ItemColumn.PROFILE]].autostart_enabled.active
  end
  
  function binds.add:on_clicked()
    local profile_name = unique_profile("New Profile")
    local entry = {
      [ItemColumn.PROFILE]   = profile_name,
      [ItemColumn.ENABLED]   = false,
      [ItemColumn.ACTIVABLE] = false,
      [ItemColumn.VISIBLE]   = true,
      [ItemColumn.WEIGHT]    = 400
    }
    local view = ProfileView(profile_name)
    item_store:append(entry)
    view_registry[profile_name] = view
    binds.stack_view:add_named(view.widget, profile_name);
  end
  
  function binds.remove:on_clicked()
    local dialog = Gtk.Dialog {
      title               = "Confirmation",
      transient_for       = window,
      modal               = true,
      destroy_with_parent = true
    }
    local byes    = dialog:add_button("Yes",    Gtk.ResponseType.YES)
    local bcancel = dialog:add_button("Cancel", Gtk.ResponseType.CANCEL)
    dialog:get_action_area().halign = Gtk.Align.CENTER
    local box = Gtk.Box {
      orientation  = 'HORIZONTAL',
      spacing      = 8,
      border_width = 8,
      Gtk.Image {
        icon_name = "dialog-warning-symbolic",
        icon_size = Gtk.IconSize.DIALOG,
      },
      Gtk.Label {
        label = "Are you sure you want to delete the selected profile?"
    } }
    dialog:get_content_area():add(box)
    box:show_all()
    local ret = dialog:run()
    dialog:set_visible(false)
    if ret ~= Gtk.ResponseType.YES then return end
    
    local model, iter = selection:get_selected()
    if model and iter then
      for iter, entry in item_store:pairs() do
        if selection:iter_is_selected(iter) then
          binds.stack_view:remove(
            binds.stack_view:get_child_by_name(
              entry[ItemColumn.PROFILE]))
          view_registry[entry[ItemColumn.PROFILE]]:delete()
          view_registry[entry[ItemColumn.PROFILE]] = nil
        end
      end
      model:remove(iter)
    end
  end
  
  function window:on_destroy() os.exit(0) end
  
  window:show_all()
  window:set_icon_from_file(glava.resource_path .. "glava.bmp")
  Gtk.main()
end
