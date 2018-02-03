in vec4 gl_FragCoord;

#request uniform "screen" screen
uniform ivec2 screen;

#request uniform "audio_sz" audio_sz
uniform int audio_sz;

#include ":bars.glsl"

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
    float /* (x, magnitude) of fragment */
        dx = (gl_FragCoord.x - (screen.x / 2)),
        d = gl_FragCoord.y;
    float nbars = floor((screen.x * 0.5F) / float(BAR_WIDTH + BAR_GAP)) * 2;
    float section = BAR_WIDTH + BAR_GAP;    /* size of section for each bar (including gap) */
    float center =  section / 2.0F;         /* half section, distance to center             */
    float m = abs(mod(dx, section));        /* position in section                          */
    float md = m - center;                  /* position in section from center line         */
    if (md < ceil(float(BAR_WIDTH) / 2) && md >= -floor(float(BAR_WIDTH) / 2)) {  /* if not in gap */
        float p = int(dx / section) / float(nbars / 2); /* position, (-1.0F, 1.0F))     */
        p += sign(p) * ((0.5F + center) / screen.x);    /* index center of bar position */
        /* Apply smooth function and index texture */
        #define smooth_f(tex, p) smooth_audio(tex, audio_sz, p)
        float v;
        /* ignore out of bounds values */
        if (p > 1.0F || p < -1.0F) {
            fragment = vec4(0, 0, 0, 0);
            return;
        }
        /* handle user options and store result of indexing in 'v' */
        if (p >= 0.0F) {
            #if DIRECTION == 1
            p = 1.0F - p;
            #endif
            #if INVERT > 0
            v = smooth_f(audio_l, p);
            #else
            v = smooth_f(audio_r, p);
            #endif
        } else {
            p = abs(p);
            #if DIRECTION == 1
            p = 1.0F - p;
            #endif
            #if INVERT > 0
            v = smooth_f(audio_r, p);
            #else
            v = smooth_f(audio_l, p);
            #endif
        }
        #undef smooth_f

        v *= AMPLIFY;                    /* amplify result                                  */
        if (d < v - BAR_OUTLINE_WIDTH) { /* if within range of the reported frequency, draw */
            #if BAR_OUTLINE_WIDTH > 0
            if (md < (BAR_WIDTH / 2) - BAR_OUTLINE_WIDTH)
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
    fragment = vec4(0, 0, 0, 0); /* default frag color */
}
