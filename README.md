
<img align="left" width="200" height="200" src="https://thumbs.gfycat.com/DefiantInformalIndianspinyloach-size_restricted.gif" />

**GLava** is an OpenGL audio spectrum visualizer. Its primary use case is for desktop windows or backgrounds. Displayed to the left is the `radial` shader module, and [here is a demonstration video](https://streamable.com/dgpj8). Development is active, and reporting issues is encouranged.

**Compiling** (Or use the [`glava-git` AUR package](https://aur.archlinux.org/packages/glava-git/))**:**

```bash
$ git clone --recursive https://github.com/wacossusca34/glava
$ cd glava
$ make
$ sudo make install
$ glava
```

You can pass `BUILD=debug` to the makefile for debug builds of both glad and glava, and you can manually specify install targets with `INSTALL=...`, possible arguments are `unix` for FHS compliant Linux and BSD distros, `osx` for Mac OSX, and `standalone` which allows you to run GLava in the build directory.

**Requirements:**

- X11
- PulseAudio
- GLFW 3.1+ (optional, disable with `DISABLE_GLFW=1`)
- Linux or BSD

**Additional compile time requirements:**

- glad (included as a submodule)
- python (required to generate bindings with glad)
- GCC (this program uses GNU C features)
- GLX headers (optional, disable direct GLX support with `DISABLE_GLX=1`), usually the development packages for `libgl` include this on your distro

**Ubuntu/Debian users:** the following command ensures you have all the needed packages and headers to compile GLava:
```bash
sudo apt-get install libpulse0 libpulse-dev libglfw3 libglfw3-dev libglx0 libxext6 libxext-dev python make gcc 
```

## [Configuration](https://github.com/wacossusca34/glava/wiki)

GLava will start by looking for an entry point in the user configuration folder (`~/.config/glava/rc.glsl`\*), and will fall back to loading from the shader installation folder (`/etc/xdg/glava`\*). The entry point will specify a module to load and should set global configuration variables. Configuration for specific modules can be done in their respective `.glsl` files, which the module itself will include.

You should start by running `glava --copy-config`. This will copy over default configuration files and create symlinks to modules in your user config folder. GLava will either load system configuration files or the user provided ones, so it's not advised to copy these files selectively.

To embed GLava in your desktop (for EWMH compliant window managers), use `#request setxwintype "desktop"` and then position it accordingly with `#request setgeometry x y width height`. You may want to also use `#request setforcegeometry true` for some window managers.

\* On an XDG compliant Linux or BSD system. OSX will use `/Library/glava` and `~/Library/Preferences/glava` instead.

## Desktop window compatibility

GLava aims to be compatible with _most_ EWMH compliant window managers. Below is a list of common window managers and issues specific to them for trying to get GLava to behave as a desktop window or widget:

| WM | ! | Details
| :---: | --- | --- |
| Mutter (GNOME, Budgie) | ![-](https://placehold.it/15/118932/000000?text=+) | `"native"` (default) opacity should be used
| Openbox (LXDE or standalone) | ![-](https://placehold.it/15/118932/000000?text=+) | [Some tweaks may be required](https://www.reddit.com/r/unixporn/comments/7vcgi4/oc_after_receiving_positive_feedback_here_i/dtrkvja/)
| Xfwm (XFCE) | ![-](https://placehold.it/15/118932/000000?text=+) | Untested, but should work without issues
| Fluxbox | ![-](https://placehold.it/15/118932/000000?text=+) | Untested, but should work without issues
| iceWM | ![-](https://placehold.it/15/118932/000000?text=+) | No notable issues
| Herbstluftwm | ![-](https://placehold.it/15/118932/000000?text=+) | `hc rule windowtype~'_NET_WM_WINDOW_TYPE_DESKTOP' manage=off` can be used to unmanage desktop windows
| AwesomeWM | ![-](https://placehold.it/15/f09c00/000000?text=+) | Can still be focused, may require other changes to config depending on layout
| kwin (KDE) | ![-](https://placehold.it/15/f09c00/000000?text=+) | [Issues with workspaces and stacking](https://github.com/wacossusca34/glava/issues/4), needs further testing
| i3 (and i3-gaps) | ![-](https://placehold.it/15/f03c15/000000?text=+) | [i3 does not respect the `"desktop"` window type](https://github.com/wacossusca34/glava/issues/6)
| EXWM | ![-](https://placehold.it/15/f03c15/000000?text=+) | EXWM does not have a desktop, and forces window decorations
| Unity | ![-](https://placehold.it/15/1589F0/000000?text=+) | Needs testing
| Enlightenment | ![-](https://placehold.it/15/1589F0/000000?text=+) | Needs testing
| Bspwm | ![-](https://placehold.it/15/1589F0/000000?text=+) | Needs testing
| xmonad | ![-](https://placehold.it/15/1589F0/000000?text=+) | Needs testing
| Any non EWMH-compliant WM | ![-](https://placehold.it/15/f03c15/000000?text=+) | Window types and hints will not work if the window manager does not support the EWMH standards.

## Licensing

GLava is licensed under the terms of the GPLv3. GLava includes some (heavily modified) source code that originated from [cava](https://github.com/karlstav/cava), which was initially provided under the MIT license. The source files that originated from cava are the following:

- `[cava]/input/fifo.c -> [glava]/fifo.c`
- `[cava]/input/fifo.h -> [glava]/fifo.h`
- `[cava]/input/pulse.c -> [glava]/pulse_input.c`
- `[cava]/input/pulse.h -> [glava]/pulse_input.h`

The below copyright notice applies for the original versions of these files:

`Copyright (c) 2015 Karl Stavestrand <karl@stavestrand.no>`

The modified files are relicensed under the terms of the GPLv3. The MIT license is included for your convience and to satisfy the requirements of the original license, although it (no longer) applies to any code in this repository. You will find the original copyright notice and MIT license in the `LICENSE_ORIGINAL` file.

The below copyright applies for the modifications to the files listed above, and the remaining sources in the repository:

`Copyright (c) 2017 Levi Webb`

## Porting

GLava was built with GLFW, making the graphics frontend mostly compatible if it were to be ported to Windows, and I have taken all the Xlib-specific code and placed it into `xwin.c` if anyone decides they wish to attempt at a port.
