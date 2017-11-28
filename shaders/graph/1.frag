
layout(pixel_center_integer) in vec4 gl_FragCoord;

#request uniform "screen" screen
uniform ivec2 screen; /* screen dimensions */

#request uniform "audio_sz" audio_sz
uniform int audio_sz;

/* How many frames to queue and run through the average function */
#request setavgframes 6

/* Whether to window frames ran through the average function (new & old frames
   are weighted less). This massively helps smoothing out spikes in the animation */
#request setavgwindow true

/* Gravity step, higher values means faster drops. The step is applied in a rate
   independant method like so:
   
   val -= (gravitystep) * (seconds per update) */
#request setgravitystep 5.2

/* When we transform our audio, we need to go through the following steps: 
   
   transform -> "window"
       First, apply a window function to taper off the ends of the spectrum, helping
       avoid artifacts in the FFT output.
   
   transform -> "fft"
       Apply the Fast Fourier Transform algorithm to separate raw audio data (waves)
       into their respective spectrums.
   
   transform -> "fft"
       As part of the FFT process, we return spectrum magnitude on a log(n) scale,
       as this is how the (decibel) dB scale functions.
       
   transform -> "gravity"
       To help make our data more pleasing to look at, we apply our data received over
       time to a buffer, taking the max of either the existing value in the buffer or
       the data from the input. We then reduce the data by the 'gravity step', and
       return the storage buffer.
       
       This makes frequent and abrupt changes in frequency less distracting, and keeps
       short frequency responses on the screen longer.
       
   transform -> "avg"
       As a final step, we take the average of several data frames (specified by
       'setavgframes') and return the result to further help smooth the resulting
       animation. In order to mitigate abrupt changes to the average, the values
       at each end of the average buffer can be weighted less with a window function
       (the same window function used at the start of this step!). It can be disabled
       with 'setavgwindow'.
*/

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
