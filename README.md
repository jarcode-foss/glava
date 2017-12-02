## GLava

GLava is an (in development) OpenGL audio spectrum visualizer. Its primary use case is for desktop widgets or backgrounds.

**Compiling** (after meeting the requirements)**:**

```bash
$ git clone --recursive https://github.com/wacossusca34/glava
$ cd glava
$ make
$ sudo make install
```

You can pass `BUILD=debug` to the makefile for debug builds of both glad and glava, and you can manually specify install targets with `INSTALL=...`, possible arguments are `unix` for FHS compliant Linux and BSD distros, `osx` for Mac OSX, and `standalone` which allows you to run glava in the build directory.

Note: GLFW's most recent stable version is 3.2, whereas 3.3 is needed in order to support transparency (older versions still work, just without transparency). You can either find a more recent build or compile it yourself. Arch users can install `glfw-x11-git` from the AUR.

**Requirements:**

- X11
- PulseAudio
- GLFW (version 3.3+ needed for transparency support)
- Linux or BSD

**Additional compile time requirements:**

- glad (included as a submodule)
- python (required to generate bindings with glad)
- GCC (this program uses GNU C features)

**What needs to be done:**

- Fix breaks in audio spectrum read from the PulseAudio server (possibly a scheduling issue?)
- Add more visualizer modules and clean up existing ones

**What is complete:**

- Core renderer
- FFT algorithm
- Programmable modules and configuration written in pure GLSL
- Preprocessor directive parsing to handle requests and transformations on uniforms before they are passed to the shader.
- Detecting if the window manager reports the currently focused window as fullscreen (and halting rendering for the duration)
- Fixed a memory corruption bug that crashed the `nvidia` driver on Linux

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