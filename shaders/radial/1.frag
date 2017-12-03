layout(pixel_center_integer) in vec4 gl_FragCoord;

#request uniform "screen" screen
uniform ivec2 screen;

#request uniform "audio_sz" audio_sz
uniform int audio_sz;

#request setavgframes 6

#request setavgwindow true

#request setgravitystep 5.2

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

#include "../radial.glsl"

#define TWOPI 6.28318530718
#define PI 3.14159265359

void main() {
    float /* translate (x, y) to use (0, 0) as the center of the screen */
        dx = gl_FragCoord.x - (screen.x / 2),
        dy = (screen.y / 2) - gl_FragCoord.y;
    float theta = atan(dy, dx); /* fragment angle with the center of the screen as the origin */
    float d = sqrt((dx * dx) + (dy * dy)); /* distance */
    if (d > C_RADIUS - (float(C_LINE) / 2F) && d < C_RADIUS + (float(C_LINE) / 2F)) {
        fragment = OUTLINE;
        return;
    } else if (d > C_RADIUS) {
        float section = (TWOPI / NBARS);       /* range (radians) for each bar */
        float m = mod(theta, section);         /* position in section (radians) */
        float center = ((TWOPI / NBARS) / 2F); /* center line angle */
        float ym = d * sin(center - m);        /* distance from center line (cartesian coords) */
        if (abs(ym) < BAR_WIDTH / 2) {         /* if within width, draw audio */
            /* texture lookup */
            float v = texture(theta > 0 ? audio_l : audio_r,
                              log(int(abs(theta) / section) / float(NBARS / 2)) / WSCALE).r * AMPLIFY;
            d -= C_RADIUS + (float(C_LINE) / 2F); /* offset to fragment distance from inner circle */
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
