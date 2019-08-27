/* Note: to only render a single channel, see `setmirror` in `rc.glsl`. */

/* Center line thickness (pixels) */
#define C_LINE 1
/* Width (in pixels) of each bar */
#define BAR_WIDTH 5
/* Width (in pixels) of each bar gap */
#define BAR_GAP 1
/* Outline width (in pixels, set to 0 to disable outline drawing) */
#define BAR_OUTLINE_WIDTH 1
/* Amplify magnitude of the results each bar displays */
#define AMPLIFY 300
/* Whether the current settings use the alpha channel;
   enabling this is required for alpha to function
   correctly on X11 with `"native"` transparency */
#define USE_ALPHA 0
/* How quickly the gradient transitions, in pixels */
#define GRADIENT 80
/* Bar color. By default this provides a blue-white gradient. */
#define COLOR @fg:mix(#3366b2, #a0a0b2, clamp(d / GRADIENT, 0, 1))
/* Outline color. By default this provides a 'glint' outline based on the bar color */
#define BAR_OUTLINE @bg:vec4(COLOR.rgb * 1.5, COLOR.a)
/* Direction that the bars are facing, 0 for inward, 1 for outward */
#define DIRECTION 0
/* Whether to switch left/right audio buffers */
#define INVERT 0
/* Whether to flip the output vertically */
#define FLIP 0
/* Whether to mirror output along `Y = X`, causing output to render on the left side of the window */
/* Use with `FLIP 1` to render on the right side */
#define MIRROR_YX 0
/* Whether to disable mono rendering when `#request setmirror true` is set in `rc.glsl`. */
#define DISABLE_MONO 0

