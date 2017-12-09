 
#ifndef _SMOOTH_GLSL /* include gaurd */
#define _SMOOTH_GLSL

#ifndef TWOPI
#define TWOPI 6.28318530718
#endif

#ifndef PI
#define PI 3.14159265359
#endif

#include ":smooth_parameters.glsl"

/* window value t that resides in range [0, sz)*/
#define window(t, sz) (0.53836 - (0.46164 * cos(TWOPI * t / (sz - 1))))
/* this does nothing, but we keep it as an option for config */
#define linear(x) (x)
/* take value x that scales linearly between [0, 1) and return its sinusoidal curve */
#define sinusoidal(x) ((0.5 * sin((PI * (x)) - (PI / 2))) + 0.5)
/* take value x that scales linearly between [0, 1) and return its circlar curve */
#define circular(x) sqrt(1 - (((x) - 1) * ((x) - 1)))

float scale_audio(float idx) {
    return -log((-(SAMPLE_RANGE) * idx) + 1) / (SAMPLE_SCALE);
}

float iscale_audio(float idx) {
    return -log((SAMPLE_RANGE) * idx) / (SAMPLE_SCALE);
}

float smooth_audio(in sampler1D tex, int tex_sz, highp float idx, float r) {
    float
        smin  = scale_audio(clamp(idx - r, 0, 1)) * tex_sz,
        smax  = scale_audio(clamp(idx + r, 0, 1)) * tex_sz,
        avg = 0, s, weight = 0;
    float m = ((smax - smin) / 2.0F);
    float rm = smin + m; /* middle */
    for (s = smin; s <= smax; s += 1.0F) {
        float w = ROUND_FORMULA(clamp((m - abs(rm - s)) / m, 0, 1));
        weight += w;
        avg += texture(tex, float(s) / float(tex_sz)).r * w;
    }
    avg /= weight;
    avg *= max(idx + 0.7, 1);
    return avg;
}

/* Applies the audio smooth sampling function three times to the adjacent values */
float smooth_audio_adj(in sampler1D tex, int tex_sz, highp float idx, float r, highp float pixel) {
    float
        al = smooth_audio(tex, tex_sz, max(idx - pixel, 0.0F), r),
        am = smooth_audio(tex, tex_sz, idx, r),
        ar = smooth_audio(tex, tex_sz, min(idx + pixel, 1.0F), r);
    return (al + am + ar) / 3.0F;
}

#ifdef TWOPI
#undef TWOPI
#endif
#ifdef PI
#undef PI
#endif
#endif /* _SMOOTH_GLSL */
