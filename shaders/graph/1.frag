
layout(pixel_center_integer) in vec4 gl_FragCoord;

#request uniform "screen" screen
uniform ivec2 screen; /* screen dimensions */

#request uniform "audio_sz" audio_sz
uniform int audio_sz;

#request setavgframes 4
#request setavgwindow true
// #request interpolate true

#request uniform "audio_l" audio_l
#request transform audio_l "window"
#request transform audio_l "fft"
#request transform audio_l "avg"
uniform sampler1D audio_l;

#request uniform "audio_r" audio_r
#request transform audio_r "window"
#request transform audio_r "fft"
#request transform audio_r "avg"
uniform sampler1D audio_r;

out vec4 fragment;

#include "settings.glsl"

#define CUT 0.5
#define SAMPLE_RANGE 0.2
#define SAMPLE_AMT 22

#define WSCALE 11
#define VSCALE 6000

#define DIRECTION 1

#define RCOL_OFF (gl_FragCoord.x / 3000)
#define LCOL_OFF ((screen.x - gl_FragCoord.x) / 3000)
#define COLOR vec4(vec3(0.3 + RCOL_OFF, 0.3, 0.3 + LCOL_OFF) + (gl_FragCoord.y / 170), 1)

#if DIRECTION < 0
#define LEFT_IDX (gl_FragCoord.x)
#define RIGHT_IDX (-gl_FragCoord.x + screen.x)
#else
#define LEFT_IDX (half_w - gl_FragCoord.x)
#define RIGHT_IDX (gl_FragCoord.x - half_w)
#endif

void main() {
    float half_w = (screen.x / 2);
    if (gl_FragCoord.x < half_w) {
        float s = 0, r;
        int t;
        for (t = -SAMPLE_AMT; t <= SAMPLE_AMT; ++t) {
            s += (texture(audio_l, log((LEFT_IDX + (t * SAMPLE_RANGE)) / (half_w)) / WSCALE).r);
        }
        s /= float(SAMPLE_AMT * 2) + 1;
        s *= VSCALE;
    
        if (gl_FragCoord.y + 1.5 <= s) {
            fragment = COLOR;
        } else {
            fragment = vec4(0, 0, 0, 0);
        }
    } else {
        float s = 0, r;
        int t;
        for (t = -SAMPLE_AMT; t <= SAMPLE_AMT; ++t) {
            s += (texture(audio_r, log((RIGHT_IDX + (t * SAMPLE_RANGE)) / (half_w)) / WSCALE).r);
        }
        s /= float(SAMPLE_AMT * 2) + 1;
        s *= VSCALE;
    
        if (gl_FragCoord.y + 1.5 <= s) {
            fragment = COLOR;
        } else {
            fragment = vec4(0, 0, 0, 0);
        }
    }
}
