
/* The module to use. A module is a set of shaders used to produce
   the visualizer. The structure for a module is the following:
   
   module_name [directory]
       1.frag [file: fragment shader],
       2.frag [file: fragment shader],
       ...
       
   Shaders are loaded in numerical order, starting at '1.frag',
   continuing indefinitely. The results of each shader (except
   for the final pass) is given to the next shader in the list
   as a 2D sampler.
   
   See documentation for more details. */
#request mod graph

/* GLFW window hints */
#request setfloating  true
#request setdecorated true
#request setfocused   true
#request setmaximized false

/* GLFW window title */
#request settitle "GLava"

/* GLFW buffer swap interval (vsync), set to '0' to prevent
   waiting for refresh, '1' (or more) to wait for the specified
   amount of frames. */
#request setswap 1

/* Frame limiter, set to the frames per second (FPS) desired or
   simple set to zero (or lower) to disable the frame limiter. */
#request setframerate 0

/* Enable/disable printing framerate every second. 'FPS' stands
   for 'Frames Per Second', and 'UPS' stands for 'Updates Per
   Second'. Updates are performed when new data is submitted
   by pulseaudio, and require transformations to be re-applied
   (thus being a good measure of how much work your CPU has to
   perform over time) */
#request setprintframes true

/* Audio buffer size to be used for processing and shaders. 
   Increasing this value can have the effect of adding 'gravity'
   to FFT output, as the audio signal will remain in the buffer
   longer.

   This value has a _massive_ effect on FFT performance and
   quality for some modules. */
#request setbufsize 4096

/* Scale down the audio buffer before any operations are 
   performed on the data. Higher values are faster.
   
   This value can affect the output of various transformations,
   since it applies (crude) averaging to the data when shrinking
   the buffer. It is reccommended to use `setsamplerate` and
   `setsamplesize` to improve performance or accuracy instead. */
#request setbufscale 1

/* PulseAudio sample rate. Lower values can add 'gravity' to
   FFT output, but can also reduce accuracy (especially for
   higher frequencies). Most hardware samples at 44100Hz.
   
   Lower sample rates also can make output more choppy, when
   not using smoothing algorithms. */
#request setsamplerate 22000

/* PulseAudio sample buffer size. Lower values result in more
   frequent audio updates (also depends on sampling rate), but
   will also require all transformations to be applied much 
   more frequently (slower) */
#request setsamplesize 1024

/* OpenGL context and GLSL shader versions, do not change unless
   you _absolutely_ know what you are doing. */
#request setversion 3 3
#request setshaderversion 330
