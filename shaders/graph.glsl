
/* Vertical scale, larger values will amplify output */
#define VSCALE 450
/* Rendering direction, either -1 (outwards) or 1 (inwards). */
#define DIRECTION 1

/* The `RCOL_OFF`, `LCOL_OFF` AND `LSTEP` definitions are used to calculate
   the `COLOR` macro definition for output. You can remove all these values
   any simply define the `COLOR` macro yourself. */

/* right color offset */
#define RCOL_OFF (gl_FragCoord.x / 3000)
/* left color offset */
#define LCOL_OFF ((screen.x - gl_FragCoord.x) / 3000)
/* vertical color step */
#define LSTEP (pos / 170)
/* actual color definition */
#define COLOR vec4((0.3 + RCOL_OFF) + LSTEP, 0.6 - LSTEP, (0.3 + LCOL_OFF) + LSTEP, 1)
/* outline color */
#define OUTLINE vec4(0.15, 0.15, 0.15, 1)
/* 1 to invert (vertically), 0 otherwise */
#define INVERT 0
