
/* Settings for smoothing functions commonly used to display FFT output */

/* The type of formula to use for weighting values when smoothing.
   Possible values:
   
   - circular     heavily rounded points
   - sinusoidal   rounded at both low and high weighted values
                    like a sine wave
   - linear       not rounded at all, just use linear distance
   */
#define ROUND_FORMULA circular

/* Factor used to scale frequencies. Lower values allows lower
   frequencies to occupy more space. */
#define SAMPLE_SCALE 8

/* The frequency range to sample. 1.0 would be the entire FFT output,
   and lower values reduce the displayed frequencies in a log-like
   scale. */
#define SAMPLE_RANGE 0.9
