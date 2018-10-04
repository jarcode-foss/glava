
/* center line thickness (pixels) */
#define C_LINE 1

/* width (in pixels) of each bar */
#define BAR_WIDTH 4
/* width (in pixels) of each bar gap */
#define BAR_GAP 2
/* outline color */
#define BAR_OUTLINE #262626
/* outline width (in pixels, set to 0 to disable outline drawing) */
#define BAR_OUTLINE_WIDTH 0
/* Amplify magnitude of the results each bar displays */
#define AMPLIFY 300
/* Alpha channel for Bars color */
#define ALPHA 0.7
/* How strong the gradient changes */
#define GRADIENT_POWER 60
/* Bar color changes with height */
#define GRADIENT (d / GRADIENT_POWER + 1)
/* Bar color */
#define COLOR (#3366b2 * GRADIENT * ALPHA)
/* Direction that the bars are facing, 0 for inward, 1 for outward */
#define DIRECTION 0
/* Whether to switch left/right audio buffers */
#define INVERT 0
/* Whether to flip the output vertically */
#define FLIP 0

