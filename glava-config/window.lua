return function()
  local lgi = require 'lgi'
  local GObject = lgi.GObject
  local Gtk = lgi.Gtk
  local Pango = lgi.Pango
  local Gdk = lgi.Gdk
  local GdkPixbuf = lgi.GdkPixbuf
  
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

  local function ComboBoxFixed(tbl)
    local inst = Gtk.ComboBoxText { id = tbl.id }
    for _, v in pairs(tbl) do
      if type(v) == "table" then
        inst:append_text(v[1])
      end
    end
    inst:set_active(tbl.default or 0)
    return inst
  end
  
  local ConfigView = function(tbl)
    local list = {}
    local idx  = 0
    for _, entry in pairs(tbl) do
      list[#list + 1] = {
        Gtk.Label { label = entry[1], xalign = 0 },
        left_attach = 0, top_attach = idx
      }
      list[#list + 1] = {
        Gtk.Alignment { xscale = 0, yscale = 0, xalign = 1, entry[2] },
        left_attach = 1, top_attach = idx
      }
      idx = idx + 1
    end
    return Gtk.ScrolledWindow {
      shadow_type = "IN",
      expand = true,
      Gtk.Alignment {
        top_padding   = 12,
        left_padding  = 20,
        right_padding = 20,
        xscale        = 1,
        yscale        = 1,
        xalign        = 0,
        Gtk.Grid {
          row_spacing        = 5,
          column_spacing     = 12,
          column_homogeneous = true,
          unpack(list)
        }
      }
    }
  end

  local ServiceView = function(self)
    local switch = Gtk.Switch { id = "autostart_enabled", sensitive = false }
    local method = ComboBoxFixed {
      { "None"                 },
      { "SystemD User Service" },
      { "InitD Entry"          },
      { "Desktop Entry"        }
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
    return ConfigView { { "Enabled", switch }, { "Autostart Method", method } }
  end
  
  local ProfileView = function(name)
    local self = { name = name }
    local notebook = Gtk.Notebook {
      expand = true,
      { tab_label = "Global Options",
        Gtk.ScrolledWindow {
          shadow_type = "IN",
          Gtk.Box {}
        }
      },
      { tab_label = "Smoothing Options",
        Gtk.ScrolledWindow {
          shadow_type = "IN",
          Gtk.Box {}
        }
      },
      { tab_label = "Module Options",
        Gtk.ScrolledWindow {
          shadow_type = "IN",
          Gtk.Box {}
        }
      },
      { tab_label = "Autostart",
        name ~= "Default" and ServiceView(self) or Gtk.Label {
          label = "Autostart options are not available for the default user profile." }
      }
    }
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

  local window = Gtk.Window {
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
          Gtk.Alignment {
            xscale = 0,
            Gtk.Box {
              homogeneous = true,
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
