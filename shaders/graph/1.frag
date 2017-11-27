
layout(pixel_center_integer) in vec4 gl_FragCoord;

#request uniform "screen" screen
uniform ivec2 screen; /* screen dimensions */

#request uniform "audio_sz" audio_sz
uniform int audio_sz;

#request setavgframes 4
#request setavgwindow true
// #request interpolate true
#request setgravitystep 0.06

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

// #include "settings.glsl"

#define CUT 0.5
#define SAMPLE_RANGE 0.2
#define SAMPLE_AMT 22

#define WSCALE 11
#define VSCALE 300

#define DIRECTION -1

#define RCOL_OFF (gl_FragCoord.x / 3000)
#define LCOL_OFF ((screen.x - gl_FragCoord.x) / 3000)
#define LSTEP (gl_FragCoord.y / 170)
#define COLOR vec4((0.3 + RCOL_OFF) + LSTEP, 0.6 - LSTEP, (0.3 + LCOL_OFF) + LSTEP, 1)

/* distance from center */
#define CDIST (abs((screen.x / 2) - gl_FragCoord.x) / screen.x)
/* distance from sides (far) */
#define FDIST (min(gl_FragCoord.x, screen.x - gl_FragCoord.x) / screen.x)

#if DIRECTION < 0
#define LEFT_IDX (gl_FragCoord.x)
#define RIGHT_IDX (-gl_FragCoord.x + screen.x)
/* distance from base frequencies */
#define BDIST CDIST
/* distance from high frequencies */
#define HDIST FDIST
#else
#define LEFT_IDX (half_w - gl_FragCoord.x)
#define RIGHT_IDX (gl_FragCoord.x - half_w)
#define BDIST FDIST
#define HDIST CDIST
#endif

float half_w;

void render_side(sampler1D tex, float idx) {
    float s = 0;
    int t;
    /* perform samples */
    for (t = -SAMPLE_AMT; t <= SAMPLE_AMT; ++t) {
        s += (texture(tex, log((idx + (t * SAMPLE_RANGE)) / half_w) / WSCALE).r);
    }
    /* compute average on samples */
    s /= float(SAMPLE_AMT * 2) + 1;
    /* scale the data upwards so we can see it */
    s *= VSCALE;
    /* clamp far ends of the screen down to make the ends of the graph smoother */
    s *= clamp((abs((screen.x / 2) - gl_FragCoord.x) / screen.x) * 48, 0.0F, 1.0F);
    s *= clamp((min(gl_FragCoord.x, screen.x - gl_FragCoord.x) / screen.x) * 48, 0.0F, 1.0F);

    /* amplify higher frequencies */
    s *= 1 + BDIST;

    /* and finally set fragment color if we are in range */
    if (gl_FragCoord.y + 1.5 <= s) {
        fragment = COLOR;
    } else {
        fragment = vec4(0, 0, 0, 0);
    }
}

void main() {
    half_w = (screen.x / 2);
    if (gl_FragCoord.x < half_w) {
        render_side(audio_l, LEFT_IDX);
    } else {
        render_side(audio_r, RIGHT_IDX);
    }
}
