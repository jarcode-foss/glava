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
      row_spacing        = 5,
      column_spacing     = 12,
      column_homogeneous = false,
      row_homogeneous    = true
    }
    local list = {}
    local idx  = 0
    local function cbuild(list, entry, idx)
      list[#list + 1] = {
        Gtk.Label { label = entry[1], halign = "START", valign = "START" },
        left_attach = 0, top_attach = idx
      }
      list[#list + 1] = {
        Gtk.Box { hexpand = true },
        left_attach = 1, top_attach = idx
      }
      list[#list + 1] = {
        apply { halign = "FILL", hexpand = false, entry[2] },
        left_attach = 2, top_attach = idx
      }
    end
    for _, entry in ipairs(tbl) do
      cbuild(list, entry, idx)
      idx = idx + 1
    end
    local adv = {}
    if tbl.advanced then
      idx = 0
      for _, entry in ipairs(tbl.advanced) do
        cbuild(adv, entry, idx)
        idx = idx + 1
      end
    end
    for k, v in pairs(grid) do
      list[k] = v
      adv[k] = v
    end
    return Gtk.ScrolledWindow {
      expand = true,
      Gtk.Box {
        margin_top  = 12,
        margin_left  = 16,
        margin_right = 16,
        hexpand      = true,
        vexpand      = true,
        halign       = "FILL",
        orientation  = "VERTICAL",
        spacing      = 6,
        Gtk.Grid(list),
        #adv > 0 and
          SpoilerView {
            label = "Show Advanced",
            Gtk.Grid(adv)
          } or Gtk.Box {}
      }
    }
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
  local widget_generators
  widget_generators = {
    ["boolean"] = function(attrs)
      local widget = Gtk.Switch { hexpand = false }
      return {
        widget = Gtk.Box { Gtk.Box { hexpand = true }, wrap_label(widget, attrs.label) },
        set_data = function(x)
          widget.active = x
        end
      }
    end,
    ["string"] = function(attrs)
      local widget = attrs.entries ~= nil
        and apply { hexpand = true, ComboBoxFixed(attrs.entries) }
        or Gtk.Entry { width_chars = 16 }
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
                return
              end
            end
            local fmt = "WARNING: Invalid string entry for Gtk.ComboBox mapping: \"%s\""
            print(string.format(fmt, x))
          end
        end
      }
    end,
    ["ident"] = function(attrs)
      local s = widget_generators.string(attrs)
      s.internal:get_style_context():add_provider(cssp, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
      s.internal:get_style_context():add_class("fixed-width-font-entry")
      if not attrs.entries then
        -- Handle idenifier formatting for entries without a preset list
        function s.internal:on_changed()
          local i = s.internal.text
          if i:match("[^%w]") ~= nil or i:sub(1, 1):match("[^%a]") ~= nil then
            s.internal.text = i:gsub("[^%w]", ""):gsub("^[^%a]+", "")
          end
          -- todo: handle changed (signal override?)
        end
      end
      return s
    end,
    ["float"] = function(attrs)
      local widget = Gtk.SpinButton {
        hexpand = true,
        adjustment = Gtk.Adjustment {
          lower          = attrs.lower or 0,
          upper          = attrs.upper or 100,
          page_size      = 1,
          step_increment = attrs.increment and (attrs.increment / 10) or 0.1,
          page_increment = attrs.increment or 1
        },
        width_chars = attrs.width or 6,
        numeric     = true,
        digits      = attrs.digits or 2, 
        climb_rate  = attrs.increment or 1
      }
      return {
        widget = wrap_label(widget, attrs.label),
        set_data = function(x) widget:set_value(x) end
      }
    end,
    ["int"] = function(attrs)
      local widget = Gtk.SpinButton {
        hexpand = true,
        adjustment = Gtk.Adjustment {
          lower          = attrs.lower or 0,
          upper          = attrs.upper or 100,
          page_size      = 1,
          step_increment = attrs.increment and math.min(math.floor((attrs.increment / 10)), 1) or 1,
          page_increment = attrs.increment or 1
        },
        width_chars = attrs.width or 6,
        numeric     = true,
        digits      = 0, 
        climb_rate  = attrs.increment or 1
      }
      return {
        widget = wrap_label(apply { vexpand = false, widget }, attrs.label),
        set_data = function(x) widget:set_value(x) end
      }
    end,
    -- The color type is the hardest to implement; as Gtk deprecated
    -- the old color chooser button, so we have to implement our own.
    -- The benefits of doing this mean we get to use the "nice" Gtk3
    -- chooser, and the button rendering itself is much better.
    ["color"] = function(attrs)
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
        }
      }
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
      widget:get_style_context():add_class("linked")
      widget = wrap_label(widget, attrs.label)
      function btn:on_clicked()
        local dialog = (use_old_chooser and Gtk.ColorSelectionDialog or Gtk.ColorChooserDialog) {
          title               = "Select Color",
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
        
        local ret = dialog:run()
        dialog:set_visible(false)
        
        if not use_old_chooser and ret == Gtk.ResponseType.OK then
          c = dialog.rgba
          entry:set_text(attrs.alpha and utils.format_color_rgba(c) or utils.format_color_rgb(c))
          area:queue_draw()
        end
      end
      function entry:on_changed()
        local s = utils.sanitize_color(entry.text)
        c = utils.parse_color_rgba(s)
        area:queue_draw()
      end
      return {
        widget = widget,
        set_data = function(x)
          local s = utils.sanitize_color(x)
          c = utils.parse_color_rgba(s)
          area:queue_draw()
          entry:set_text(s)
        end
      }
    end
  }
  
  local ServiceView = function(self)
    local switch = Gtk.Switch {
      id        = "autostart_enabled",
      sensitive = false,
      hexpand   = false
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
    }
  end
  
  local ProfileView = function(name)
    local self = { name = name }
    local args = {}
    for k, v in pairs(mappings) do
      local layout = {}
      for _, e in ipairs(v) do
        if type(e) == "table" then
          local fields = {}
          local ftypes = type(e.field_type) == "table" and e.field_type or { e.field_type }
          local fattrs = type(e.field_type) == "table" and e.field_attrs or { e.field_attrs }
          if not fattrs then fattrs = {} end
          for i, f in ipairs(ftypes) do
            local entry = widget_generators[f](fattrs[i] or {})
            fields[#fields + 1] = entry.widget
          end
          fields.orientation = "VERTICAL"
          fields.spacing = 2
          local fwidget = {
            e.description,
            #fields > 1 and
              Gtk.Frame {
                label = fattrs.frame_label,
                apply {
                  margin_left   = 4,
                  margin_right  = 4,
                  margin_top    = 4,
                  margin_bottom = 4,
                  Gtk.Box(fields)
                }
              } or Gtk.Box(fields)
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
    args[#args + 1] = {
      tab_label = "Autostart",
      name ~= "Default" and ServiceView(self) or
        Gtk.Label {
          label = "Autostart options are not available for the default user profile."
        }
    }
    args.expand = true
    notebook = Gtk.Notebook(args)
    notebook:show_all()
    self.widget = notebook
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
      spacing = 8,
      homogeneous = false,
      Gtk.Box {
        orientation = "VERTICAL",
        spacing = 5,
        Gtk.ScrolledWindow {
          shadow_type = "ETCHED_IN",
          vexpand = true,
          width_request = 200,
          Gtk.TreeView {
            id = "view",
            model = item_store,
            activate_on_single_click = true,
            Gtk.TreeViewColumn {
              title  = "Profile",
              expand = true,
              {
                Gtk.CellRendererText {
                  id = "profile_renderer"
                },
                {
                  text     = ItemColumn.PROFILE,
                  editable = ItemColumn.VISIBLE,
                  weight   = ItemColumn.WEIGHT
                }
              }
            },
            Gtk.TreeViewColumn {
              title     = "Enabled",
              alignment = 0.5,
              {
                Gtk.CellRendererToggle {
                  id = "toggle_renderer",
                  xalign = 0.5
                },
                {
                  active      = ItemColumn.ENABLED,
                  activatable = ItemColumn.ACTIVABLE,
                  visible     = ItemColumn.VISIBLE
                }
              }
            }
          }
        },
        Gtk.Box {
          orientation = "HORIZONTAL",
          spacing = 4,
          apply {
            hexpand     = false,
            homogeneous = true,
            (function()
                local box = Gtk.Box {
                  Gtk.Button {
                    id    = "reload",
                    label = "Reload",
                    image = Gtk.Image { stock = Gtk.STOCK_REFRESH }
                  },
                  Gtk.Button {
                    id    = "add",
                    label = "New",
                    image = Gtk.Image { stock = Gtk.STOCK_NEW },
                  },
                  Gtk.Button {
                    id        = "remove",
                    label     = "Delete",
                    sensitive = false,
                    image     = Gtk.Image { stock = Gtk.STOCK_DELETE },
                  }
                }
                box:get_style_context():add_class("linked")
                return box
            end)()
          }
        },
      },
      Gtk.Stack {
        id = "stack_view",
        expand = true,
        transition_type = Gtk.StackTransitionType.CROSSFADE
      }
    }
  }

  local selection = window.child.view:get_selection()
  selection.mode = 'SINGLE'
  window.child.stack_view:add_named(view_registry[default_entry[ItemColumn.PROFILE]].widget,
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
  
  function window.child.view:on_row_activated(path, column)
    local name = item_store[path][ItemColumn.PROFILE]
    window.child.stack_view:set_visible_child_name(name)
    window.child.remove.sensitive = (name ~= "Default")
  end

  function window.child.profile_renderer:on_edited(path_string, new_profile)
    local path = Gtk.TreePath.new_from_string(path_string)
    local old = item_store[path][ItemColumn.PROFILE]
    local store = window.child.stack_view:get_child_by_name(old)
    new_profile = string.match(new_profile, "^%s*(.-)%s*$")
    if old == new_profile or new_profile == "Default" then return end
    new_profile = unique_profile(new_profile)
    print("Renamining profile \"" .. old .. "\" -> \"" .. new_profile .. "\"")
    window.child.stack_view:remove(store)
    window.child.stack_view:add_named(store, new_profile)
    local vstore = view_registry[old]
    view_registry[old] = nil
    view_registry[new_profile] = vstore
    vstore:rename(new_profile)
    item_store[path][ItemColumn.PROFILE] = new_profile
  end
  
  function window.child.toggle_renderer:on_toggled(path_string)
    local path = Gtk.TreePath.new_from_string(path_string)
    if view_registry[item_store[path][ItemColumn.PROFILE]].widget.child.autostart_enabled.active
    ~= not item_store[path][ItemColumn.ENABLED] then
      view_registry[item_store[path][ItemColumn.PROFILE]].widget.child.autostart_enabled:activate()
    end
    item_store[path][ItemColumn.ENABLED] =
      view_registry[item_store[path][ItemColumn.PROFILE]].widget.child.autostart_enabled.active
  end
  
  function window.child.add:on_clicked()
    local profile_name = unique_profile("New Profile")
    local entry = {
      [ItemColumn.PROFILE]   = profile_name,
      [ItemColumn.ENABLED]   = false,
      [ItemColumn.ACTIVABLE] = false,
      [ItemColumn.VISIBLE]   = true
    }
    local view = ProfileView(profile_name)
    item_store:append(entry)
    view_registry[profile_name] = view
    window.child.stack_view:add_named(view.widget, profile_name);
  end
  
  function window.child.remove:on_clicked()
    local dialog = Gtk.Dialog {
      title               = "Confirmation",
      transient_for       = window,
      modal               = true,
      destroy_with_parent = true
    }
    local byes    = dialog:add_button(Gtk.STOCK_YES,    Gtk.ResponseType.YES)
    local bcancel = dialog:add_button(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL)
    dialog:get_action_area().halign = Gtk.Align.CENTER
    local box = Gtk.Box {
      orientation  = 'HORIZONTAL',
      spacing      = 8,
      border_width = 8,
      Gtk.Image {
        stock     = Gtk.STOCK_DIALOG_WARNING,
        icon_size = Gtk.IconSize.DIALOG,
      },
      Gtk.Label {
        label = "Are you sure you want to delete the selected profile?"
      }
    }
    dialog:get_content_area():add(box)
    box:show_all()
    local ret = dialog:run()
    dialog:set_visible(false)
    if ret ~= Gtk.ResponseType.YES then return end
    
    local model, iter = selection:get_selected()
    if model and iter then
      for iter, entry in item_store:pairs() do
        if selection:iter_is_selected(iter) then
          window.child.stack_view:remove(
            window.child.stack_view:get_child_by_name(
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
