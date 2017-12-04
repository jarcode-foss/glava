## GLava
<img align="left" width="240" height="240" src="https://thumbs.gfycat.com/VibrantInsistentFowl-size_restricted.gif" \>
GLava is an (in development) OpenGL audio spectrum visualizer. Its primary use case is for desktop widgets or backgrounds. Displayed to the left is the `radial` shader module running in GLava.

**Compiling** (after meeting the requirements)**:**

```bash
$ git clone --recursive https://github.com/wacossusca34/glava
$ cd glava
$ make
$ sudo make install
$ glava
```

You can pass `BUILD=debug` to the makefile for debug builds of both glad and glava, and you can manually specify install targets with `INSTALL=...`, possible arguments are `unix` for FHS compliant Linux and BSD distros, `osx` for Mac OSX, and `standalone` which allows you to run GLava in the build directory.

**Note:** GLFW's most recent stable version is 3.2, whereas 3.3 is needed in order to support transparency (older versions still work, just without transparency). You can either find a more recent build or compile it yourself. Arch users can install `glfw-x11-git` from the AUR.

**Requirements:**

- X11
- PulseAudio
- GLFW (version 3.3+ needed for transparency support)
- Linux or BSD

**Additional compile time requirements:**

- glad (included as a submodule)
- python (required to generate bindings with glad)
- GCC (this program uses GNU C features)

## Configuration

GLava will start by looking for an entry point in the user configuration folder (`~/.config/glava/rc.glsl`\*), and will fall back to loading from the shader installation folder (`/etc/xdg/glava`\*). The entry point will specify a module to load and should set global configuration variables. Configuration for specific modules can be done in their respective `.glsl` files, which the module itself will include.

You should start by running `glava --copy-config`. This will copy over default configuration files and create symlinks to modules in your user config folder. GLava will either load system configuration files or the user provided ones, so it's not advised to copy these files selectively.

To embed GLava in your desktop (for EWMH compliant window managers), use `#request setxwintype "desktop"` and then position it accordingly with `#request setgeometry x y width height`. You may want to also use `#request setforcegeometry true` for some window managers.

\* On an XDG compliant Linux or BSD system. OSX will use `/Library/glava` and `~/Library/Preferences/glava` instead.

## To-Do

**What needs to be done:**

- Add more visualizer modules and clean up existing ones

**What is complete:**

- Core renderer
- FFT algorithm
- Programmable modules and configuration written in pure GLSL
- Preprocessor directive parsing to handle requests and transformations on uniforms before they are passed to the shader.
- Detecting if the window manager reports the currently focused window as fullscreen (and halting rendering for the duration)
- Fixed a memory corruption bug that crashed the `nvidia` driver on Linux
- Fix breaks in audio spectrum read from the PulseAudio server

**What will never be done:**

- Port to Windows (???)

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

**Why did you relicense this software?** Because my views align with the FSF's position on free software -- I see the MIT license as harmful to free software as it fails to preserve modifications of the source code as open source.