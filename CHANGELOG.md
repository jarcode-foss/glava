## 1.4

* Added window creation backends. Currently GLava automatically uses the appropriate backend, currently `"glx"` and `"glfw"` are available. The GLX window creation code nicely supports transparent windows so that users do not need to use GLFW 3.3 for native transparency, which is currently unstable and unreleased.
* Added a `--backend` (or `-b`) flag that can be used to manually select window creation backends.
* Fixed alpha blending with `"native"` transparency. GLava now correctly uses premultiplied alpha to provide alpha blending with other X windows, where a compositor is available.
* Fixed a segmentation fault that occurs with uncommon window managers, see #19
* Fixed various other issues with alpha blending in shader code.

## 1.3

* Added RGBA color parsing for GLSL code and directives (does not work within `#request` directives). The behaviour converts `#3ef708ff` -> `vec4(0.243, 0.967, 0.031, 1.0)`. If the alpha component is omitted, it is assumed to be `1.0`. The parsing occurs during compile time, so it does not incurr any performance penalty.
* Fixed stray `[DEBUG]` messages that were errornously being printed
* _Actually_ added support for GLFW 3.1, see issue #13
* Fixed issue with some WMs not responding to GLava's window types and hints, see #17
* Fixed alpha blending not working, `radial` and `circle` image quality should be drastically improved on `xroot` and `none` opacity settings.
* Added `--version` command line flag

## 1.2

* Attempted to add support for GLFW 3.1, see issue #13
* Added configurable solid background color, see #16
* Fixed issue with excessive file reads from `~/.Xauthority` see #15

## 1.1

* Added `circle` module
* Added anti-aliasing to the `radial` module, written into the shader itself
* Default configuration values should create more appealing visualizers now
* Massive performance improvements, see issue #10

## 1.0

* All versions of GLava with `1.0` version numbers are older development releases.
