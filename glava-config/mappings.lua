return {
  ["rc.glsl"] = {
    name = "Global Options",
    {
      "request:mod",
      field_type = "string",
      field_attrs = {
        entries = glava.module_list
      },
      description = "Visualizer module"
    },
    {
      "request:fakeident",
      field_type = "ident",
      description = "Some identifier"
    },
    {
      "request:fakefloat",
      field_type = "float",
      description = "Some Float"
    },
    {
      "request:setbg",
      field_type = "color",
      field_attrs = { alpha = true },
      description = "Window background color"
    },
    "advanced",
    {
      "request:setversion",
      field_type = { "int", "int" },
      field_attrs = {
        frame_label = "Version",
        { label = "Major:", lower = 0, upper = 10, width = 2 },
        { label = "Minor:", lower = 0, upper = 10, width = 2 }
      },
      description = "OpenGL context version request"
    }
  }
}
