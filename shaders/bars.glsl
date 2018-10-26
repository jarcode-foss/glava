#include ":bars-user.glsl"

/* Center line thickness (pixels) */
#ifndef C_LINE
    #define C_LINE 1
#endif

/* Width (in pixels) of each bar */
#ifndef BAR_WIDTH
    #define BAR_WIDTH 4
#endif

/* Width (in pixels) of each bar gap */
#ifndef BAR_GAP
    #define BAR_GAP 2
#endif

/* Outline color */
#ifndef BAR_OUTLINE
    #define BAR_OUTLINE #262626
#endif

/* Outline width (in pixels, set to 0 to disable outline drawing) */
#ifndef BAR_OUTLINE_WIDTH
    #define BAR_OUTLINE_WIDTH 0
#endif

/* Amplify magnitude of the results each bar displays */
#ifndef AMPLIFY
    #define AMPLIFY 300
#endif

/* Alpha channel for bars color */
#ifndef ALPHA
    #define ALPHA 0.7
#endif

/* How strong the gradient changes */
#ifndef GRADIENT_POWER
    #define GRADIENT_POWER 60
#endif

/* Bar color changes with height */
#ifndef GRADIENT
    #define GRADIENT (d / GRADIENT_POWER + 1)
#endif

/* Bar color */
#ifndef COLOR
    #define COLOR (#3366b2 * GRADIENT * ALPHA)
#endif

/* Direction that the bars are facing, 0 for inward, 1 for outward */
#ifndef DIRECTION
    #define DIRECTION 0
#endif

/* Whether to switch left/right audio buffers */
#ifndef INVERT
    #define INVERT 0
#endif

/* Whether to flip the output vertically */
#ifndef FLIP
    #define FLIP 0
#endif


/* Whether to mirror output along `Y = X`, causing output to render on the left side of the window */
/* Use with `FLIP 1` to render on the right side */
#ifndef MIRROR_YX
    #define MIRROR_YX 0
#endif

