/* min (vertical) line thickness */
#define MIN_THICKNESS 1

/* max (vertical) line thickness */
#define MAX_THICKNESS 6

/* base color to use, distance from center will multiply the RGB components */
#if USE_STDIN == 1
#define BASE_COLOR STDIN
#else
#define BASE_COLOR vec4(0.7, 0.2, 0.45, 1)
#endif

/* amplitude */
#define AMPLIFY 500

/* outline color */
#define OUTLINE vec4(0.15, 0.15, 0.15, 1)
