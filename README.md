
<img align="left" width="200" height="200" src="https://thumbs.gfycat.com/DefiantInformalIndianspinyloach-size_restricted.gif" />

**GLava** is an OpenGL audio spectrum visualizer. Its primary use case is for desktop windows or backgrounds. Displayed to the left is the `radial` shader module, and [here is a demonstration video](https://streamable.com/dgpj8). Development is active, and reporting issues is encouranged.

**Compiling** (Or use the Arch Linux [`glava` package](https://www.archlinux.org/packages/community/x86_64/glava/), or the [`glava-git` AUR package](https://aur.archlinux.org/packages/glava-git/))**:**

```bash
$ git clone https://github.com/wacossusca34/glava
$ cd glava
$ CFLAGS="-march=native" make
$ sudo make install
$ glava
```

You can pass `BUILD=debug` to the makefile for debug builds of both glad and glava, and you can manually specify install targets with `INSTALL=...`, possible arguments are `unix` for FHS compliant Linux and BSD distros, `osx` for Mac OSX, and `standalone` which allows you to run GLava in the build directory.

**Requirements:**

- X11 (Xext, Xcomposite, & Xrender)
- PulseAudio
- Linux or BSD

**Additional compile time requirements:**

- GCC (this program uses GNU C features)

**Optional requirements:**

- GLFW 3.1+ (optional, enable with `ENABLE_GLFW=1`)

**Ubuntu/Debian users:** the following command ensures you have all the needed packages and headers to compile GLava:
```bash
sudo apt-get install libpulse0 libpulse-dev libglfw3 libglfw3-dev libxext6 libxext-dev libxcomposite-dev python make gcc 
```

## [Configuration](https://github.com/wacossusca34/glava/wiki)

GLava will start by looking for an entry point in the user configuration folder (`~/.config/glava/rc.glsl`\*), and will fall back to loading from the shader installation folder (`/etc/xdg/glava`\*). The entry point will specify a module to load and should set global configuration variables. Configuration for specific modules can be done in their respective `.glsl` files, which the module itself will include.

You should start by running `glava --copy-config`. This will copy over default configuration files and create symlinks to modules in your user config folder. GLava will either load system configuration files or the user provided ones, so it's not advised to copy these files selectively.

To embed GLava in your desktop (for EWMH compliant window managers), run it with the `--desktop` flag and then position it accordingly with `#request setgeometry x y width height` in your `rc.glsl`.

\* On an XDG compliant Linux or BSD system.

## Desktop window compatibility

GLava aims to be compatible with _most_ EWMH compliant window managers. Below is a list of common window managers and issues specific to them for trying to get GLava to behave as a desktop window or widget:

| WM | ! | Details
| :---: | --- | --- |
| Mutter (GNOME, Budgie) | ![-](https://placehold.it/15/118932/000000?text=+) | `"native"` (default) opacity should be used
| KWin (KDE) | ![-](https://placehold.it/15/118932/000000?text=+) | "Show Desktop" [temporarily hides GLava](https://github.com/wacossusca34/glava/issues/4#issuecomment-419729184)
| Openbox (LXDE or standalone) | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| Xfwm (XFCE) | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| Fluxbox | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| IceWM | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| Bspwm | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| Herbstluftwm | ![-](https://placehold.it/15/118932/000000?text=+) | `hc rule windowtype~'_NET_WM_WINDOW_TYPE_DESKTOP' manage=off` can be used to unmanage desktop windows
| Unity | ![-](https://placehold.it/15/118932/000000?text=+) | No issues
| AwesomeWM | ![-](https://placehold.it/15/118932/000000?text=+) | Defaults to unmanaged
| i3 (and i3-gaps) | ![-](https://placehold.it/15/118932/000000?text=+) | Defaults to unmanaged
| EXWM | ![-](https://placehold.it/15/f03c15/000000?text=+) | EXWM does not have a desktop, and forces window decorations
| Enlightenment | ![-](https://placehold.it/15/1589F0/000000?text=+) | Needs testing
| Xmonad | ![-](https://placehold.it/15/1589F0/000000?text=+) | Needs testing
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

## Licensing

GLava is licensed under the terms of the GPLv3, with the exemption of `khrplatform.h`, which is licensed under the terms in its header. GLava includes some (heavily modified) source code that originated from [cava](https://github.com/karlstav/cava), which was initially provided under the MIT license. The source files that originated from cava are the following:

- `[cava]/input/fifo.c -> [glava]/fifo.c`
- `[cava]/input/fifo.h -> [glava]/fifo.h`
- `[cava]/input/pulse.c -> [glava]/pulse_input.c`
- `[cava]/input/pulse.h -> [glava]/pulse_input.h`

The below copyright notice applies for the original versions of these files:

`Copyright (c) 2015 Karl Stavestrand <karl@stavestrand.no>`

The modified files are relicensed under the terms of the GPLv3. The MIT license is included for your convience and to satisfy the requirements of the original license, although it no longer applies to any code in this repository. You will find the original copyright notice and MIT license in the `LICENSE_ORIGINAL` file.

The below copyright applies for the modifications to the files listed above, and the remaining sources in the repository:

`Copyright (c) 2017 Levi Webb`

## Porting

GLava was built with GLFW, making the graphics frontend mostly compatible if it were to be ported to Windows, and I have taken most of the Xlib-specific code and placed it into `xwin.c` if anyone decides they wish to attempt at a port.
