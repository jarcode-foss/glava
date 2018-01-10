layout(pixel_center_integer) in vec4 gl_FragCoord;

#request uniform "screen" screen
uniform ivec2 screen;

#request uniform "audio_sz" audio_sz
uniform int audio_sz;

#include ":radial.glsl"

#request uniform "audio_l" audio_l
#request transform audio_l "window"
#request transform audio_l "fft"
#request transform audio_l "gravity"
#request transform audio_l "avg"
uniform sampler1D audio_l;

#request uniform "audio_r" audio_r
#request transform audio_r "window"
#request transform audio_r "fft"
#request transform audio_r "gravity"
#request transform audio_r "avg"
uniform sampler1D audio_r;

out vec4 fragment;
#include ":util/smooth.glsl"

#define TWOPI 6.28318530718
#define PI 3.14159265359

void main() {
    float /* translate (x, y) to use (0, 0) as the center of the screen */
        dx = gl_FragCoord.x - (screen.x / 2),
        dy = gl_FragCoord.y - (screen.y / 2);
    float theta = atan(dy, dx); /* fragment angle with the center of the screen as the origin */
    float d = sqrt((dx * dx) + (dy * dy)); /* distance */
    if (d > C_RADIUS - (float(C_LINE) / 2.0F) && d < C_RADIUS + (float(C_LINE) / 2.0F)) {
        fragment = OUTLINE;
        return;
    } else if (d > C_RADIUS) {
        const float section = (TWOPI / NBARS);         /* range (radians) for each bar */
        const float center = ((TWOPI / NBARS) / 2.0F); /* center line angle */
        float m = mod(theta, section);                 /* position in section (radians) */
        float ym = d * sin(center - m);                /* distance from center line (cartesian coords) */
        if (abs(ym) < BAR_WIDTH / 2) {                 /* if within width, draw audio */
            float idx = theta + ROTATE;                /* position (radians) in texture */
            float dir = mod(abs(idx), TWOPI);          /* absolute position, [0, 2pi) */
            if (dir > PI)
                idx = -sign(idx) * (TWOPI - dir);      /* Re-correct position values to [-pi, pi) */
            if (INVERT > 0)
                idx = -idx;                            /* Invert if needed */
            float pos = int(abs(idx) / section) / float(NBARS / 2);        /* bar position, [0, 1) */
            #define smooth_f(tex) smooth_audio(tex, audio_sz, pos, SMOOTH) /* smooth function format */
            float v;
            if (idx > 0) v = smooth_f(audio_l);                            /* left buffer */
            else         v = smooth_f(audio_r);                            /* right buffer */
            v *= AMPLIFY * (1 + pos);                                      /* amplify and scale with frequency */
            #undef smooth_f
            d -= C_RADIUS + (float(C_LINE) / 2.0F); /* offset to fragment distance from inner circle */
            if (d <= v - BAR_OUTLINE_WIDTH) {
                #if BAR_OUTLINE_WIDTH > 0
                if (abs(ym) < (BAR_WIDTH / 2) - BAR_OUTLINE_WIDTH)
                    fragment = COLOR;
                else
                    fragment = BAR_OUTLINE;
                #else
                fragment = COLOR;
                #endif
                return;
            }
            #if BAR_OUTLINE_WIDTH > 0
            if (d <= v) {
                fragment = BAR_OUTLINE;
                return;
            }
            #endif
        }
    }
    fragment = vec4(0, 0, 0, 0); /* default frag color */
}
