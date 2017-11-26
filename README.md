# GLava

GLava is an (in development) OpenGL audio spectrum visualizer. Its primary use case is for desktop widgets or backgrounds.

Compiling (after meeting the requirements):

```
git clone --recursive https://github.com/wacossusca34/glava
cd glava
make
./glava
```

There are currently no make rules (or configure script) for installing anywhere else on the system, and the shaders are hardcoded to be read from the current directory. When development is complete, I will finish the configure & build process.

Requirements:

- X11
- PulseAudio
- GLFW
- Linux or BSD

Additional compile time requirements:

- glad (included as a submodule)
- python (required to generate bindings with glad)
- GCC (this program uses GNU C features)

What needs to be done:

- Fix breaks in audio spetrum read from the PulseAudio server (possibly a scheduling issue?)
- Fix OpenGL code from crashing the `nvidia` driver on Linux (???)

What is complete:

- Core renderer
- FFT algorithm
- Programmable modules and configuration written in pure GLSL
- Preprocessor directive parsing to handle requests and transformations on uniforms before they are passed to the shader.
- Detecting if the window manager reports the currently focused window as fullscreen (and halting rendering for the duration)

What will never be done:

- Port to Windows (???)