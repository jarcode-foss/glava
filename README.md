
<img align="left" width="200" height="200" src="https://thumbs.gfycat.com/DefiantInformalIndianspinyloach-size_restricted.gif" />

**GLava** is a general-purpose, highly configurable OpenGL audio spectrum visualizer for X11. Displayed to the left is the `radial` shader module, or for a more extensive demonstration [see this demo](https://streamable.com/dgpj8). Development is active, and reporting issues is encouranged.

**Compiling:**

```bash
$ git clone https://github.com/jarcode-foss/glava
$ cd glava
$ meson build --prefix /usr
$ ninja -C build
$ sudo ninja -C build install
```

You can pass `-Dbuildtype=debug` to Meson for debug builds of glava, and `-Dstandalone=true` to run glava directly from the `build` directory.

Note that versions since `2.0` use Meson for the build system, although the `Makefile` will remain to work identically to earlier `1.xx` releases (with new features disabled). Package maintainers are encouraged to use Meson directly instead of the Make wrapper.

**Requirements:**

- X11 (Xext, Xcomposite, & Xrender)
- PulseAudio
- Linux or BSD
- libBlocksRuntime if compiling with Clang

**Configuration tool requirements:**

- Lua (5.3 by default, change with `-Dlua_version=...`), and the following lua libraries:
  - Lua GObject Introspection (LGI)
  - LuaFilesystem (LFS)
- GTK+ 3

**Additional compile time requirements:**

- Meson
- OBS (disable with `-Ddisable_obs=true`)

**Optional requirements:**

- GLFW 3.1+ (optional, enable with `-Denable_glfw=true`)

**Ubuntu/Debian users:** the following command ensures you have all the needed packages and headers to compile GLava with the default feature set:
```bash
sudo apt-get install libgl1-mesa-dev libpulse0 libpulse-dev libxext6 libxext-dev libxrender-dev libxcomposite-dev liblua5.3-dev liblua5.3 lua-lgi lua-filesystem libobs0 libobs-dev meson build-essential gcc 
```
Don't forget to run `sudo ldconfig` after installing.

## Installation
Some distributions have a package for `glava`. If your distribution is not listed please use the compilation instructions above.

- Arch Linux [`glava` package](https://www.archlinux.org/packages/community/x86_64/glava/), or [`glava-git` AUR package](https://aur.archlinux.org/packages/glava-git/)
- NixOS [package](https://github.com/NixOS/nixpkgs/blob/release-18.09/pkgs/applications/misc/glava/default.nix)
- openSUSE [package](https://build.opensuse.org/package/show/X11:Utilities/glava)

## [Configuration](https://github.com/jarcode-foss/glava/wiki)

GLava will start by looking for an entry point in the user configuration folder (`~/.config/glava/rc.glsl`), and will fall back to loading from the shader installation folder (`/etc/xdg/glava`). The entry point will specify a module to load and should set global configuration variables. Configuration for specific modules can be done in their respective `.glsl` files, which the module itself will include.

You should start by running `glava --copy-config`. This will copy over default configuration files and create symlinks to modules in your user config folder. GLava will either load system configuration files or the user provided ones, so it's not advised to copy these files selectively.

To embed GLava in your desktop (for EWMH compliant window managers), run it with the `--desktop` flag and then position it accordingly with `#request setgeometry x y width height` in your `rc.glsl`.

For more information, see the [main configuration page](https://github.com/jarcode-foss/glava/wiki).

## Desktop window compatibility

GLava aims to be compatible with _most_ EWMH compliant window managers. Below is a list of common window managers and issues specific to them for trying to get GLava to behave as a desktop window or widget:

| WM | ! | Details
| :---: | --- | --- |
| Mutter (GNOME, Budgie) | ![-](https://placehold.it/15/118932/000000?text=+) | `"native"` (default) opacity should be used
| KWin (KDE) | ![-](https://placehold.it/15/118932/000000?text=+) | "Show Desktop" [temporarily hides GLava](https://github.com/jarcode-foss/glava/issues/4#issuecomment-419729184)
| Openbox (LXDE or standalone) | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| Xfwm (XFCE) | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| Fluxbox | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| IceWM | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| Bspwm | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| SpectrWM |
| Herbstluftwm | ![-](https://placehold.it/15/118932/000000?text=+) | `hc rule windowtype~'_NET_WM_WINDOW_TYPE_DESKTOP' manage=off` can be used to unmanage desktop windows
| Unity | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| AwesomeWM | ![-](https://placehold.it/15/118932/000000?text=+) | Defaults to unmanaged
| i3 (and i3-gaps) | ![-](https://placehold.it/15/118932/000000?text=+) | Defaults to unmanaged
| spectrwm | ![-](https://placehold.it/15/118932/000000?text=+) | Defaults to unmanaged
| EXWM | ![-](https://placehold.it/15/f03c15/000000?text=+) | EXWM does not have a desktop, and forces window decorations
| Enlightenment | ![-](https://placehold.it/15/1589F0/000000?text=+) | Needs testing
| Xmonad | ![-](https://placehold.it/15/118932/000000?text=+) | No issues after enabling ewmh hints via `XMonad.Hooks.EwmhDesktops.ewmh`
| Any non EWMH-compliant WM | ![-](https://placehold.it/15/f03c15/000000?text=+) | Window types and hints will not work if the window manager does not support the EWMH standards.

Note that some WMs listed without issues have specific overrides when using the `--desktop` flag. See `shaders/env_*.glsl` files for details.

## Reading from MPD's FIFO output

Add the following to your `~/.config/mpd.conf`:

```
audio_output {
    type                    "fifo"
    name                    "glava_fifo"
    path                    "/tmp/mpd.fifo"
    format                  "22050:16:2"
}
```

Note the `22050` sample rate -- this is the reccommended setting for GLava. Restart MPD (if nessecary) and start GLava with `glava --audio=fifo`.

## Using GLava with OBS

GLava installs a plugin for rendering directly to an OBS scene, if support was enabled at compile-time. This is enabled by default in Meson, but it is overridden to disabled in the `Makefile` for build compatibility.

To use the plugin, simply select `GLava Direct Source` from the source list in OBS and position the output accordingly. You can provide options to GLava in the source properties.

Note that this only works for the default GLX builds of both OBS and GLava. This feature will not work if OBS was compiled with EGL for context creation, or if GLava is using GLFW.

## Performance

GLava will have a notable performance impact by default due to reletively high update rates, interpolation, and smoothing. Because FFT computations are (at the moment) performed on the CPU, you may wish to _lower_ `setsamplesize` and `setbufsize` on old hardware.

However, there is functionality to prevent GLava from unessecarily eating resources. GLava will always halt completely when obscured, so a fullscreen application covering the visualizer should enounter no issues (ie. games). If you wish for GLava to halt rendering when _any_ fullscreen application is in focus regardless of visibility, you can set `setfullscreencheck` to `true` in `rc.glsl`.

Any serious performance and/or updating issues (low FPS/UPS) should be reported. At a minimum, modules should be expected to run smoothly on Intel HD graphics and software rasterizers like `llvmpipe`.

## Licensing

GLava is licensed under the terms of the GPLv3, with the exemption of `khrplatform.h`, which is licensed under the terms in its header. GLava includes some (heavily modified) source code that originated from [cava](https://github.com/karlstav/cava), which was initially provided under the MIT license. The source files that originated from cava are the following:

- `[cava]/input/fifo.c -> [glava]/fifo.c`
- `[cava]/input/fifo.h -> [glava]/fifo.h`
- `[cava]/input/pulse.c -> [glava]/pulse_input.c`
- `[cava]/input/pulse.h -> [glava]/pulse_input.h`

The below copyright notice applies for the original versions of these files:

`Copyright (c) 2015 Karl Stavestrand <karl@stavestrand.no>`

GLava also contains GLFFT, an excellent FFT implementation using Opengl 4.3 compute shaders. This was also initiallly provided under the MIT license, and applies to the following source files (where `*` refers to both `hpp` and `cpp`):

- `glfft/glfft.*`
- `glfft/glfft_common.hpp`
- `glfft/glfft_gl_interface.*`
- `glfft/glfft_interface.hpp`
- `glfft/glfft_wisdom.*`

The below copyright notice applies for the original versions of these files:

`Copyright (c) 2015 Hans-Kristian Arntzen <maister@archlinux.us>`

**The noted files above are all sublicensed under the terms of the GPLv3**. The MIT license is included for your convenience and to satisfy the requirements of the original license, although it no longer applies to any code in this repository. You will find the original copyright notice and MIT license in the `LICENSE_ORIGINAL` file for cava, or `glfft/LICENSE_ORIGINAL` for GLFFT.

The below copyright applies for the modifications to the files listed above, and the remaining sources in the repository:

`Copyright (c) 2017 Levi Webb`
