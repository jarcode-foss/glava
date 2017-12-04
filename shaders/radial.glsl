
/* center radius (pixels) */
#define C_RADIUS 64
/* center line thickness (pixels) */
#define C_LINE 1.5
/* outline color */
#define OUTLINE vec4(0.20, 0.20, 0.20, 1)
/* number of bars (use even values for best results) */
#define NBARS 92
/* width (in pixels) of each bar*/
#define BAR_WIDTH 3.5
/* outline color */
#define BAR_OUTLINE OUTLINE
/* outline width (in pixels, set to 0 to disable outline drawing) */
#define BAR_OUTLINE_WIDTH 0
/* Amplify magnitude of the results each bar displays */
#define AMPLIFY 300
/* Bar color */
#define COLOR (vec4(0.8, 0.2, 0.2, 1) * ((d / 40) + 1))
/* Angle (in radians) for how much to rotate the visualizer */
#define ROTATE (PI / 2)
/* Whether to switch left/right audio buffers */
#define INVERT 0
/* Smoothing factor, in normalized width */
#define SMOOTH 0.025
