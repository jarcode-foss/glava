out vec4 fragment;
in vec4 gl_FragCoord;

#include ":util/common.glsl"

/*
  This averaging shader uses compile-time loop generation to ensure two things:
  
  - We can avoid requiring GL 4.3 features to dynamically index texture arrays
  - We ensure no branching occurs in this shader for optimial performance.
  
  The alternative is requiring the GLSL compiler to determine that a loop for
  texture array indexes (which must be determined at compile-time in 3.3) can be
  expanded if the bounds are constant. This is somewhat vendor-specific so GLava
  provides a special `#expand` macro to solve this problem in the preprocessing
  stage.
*/

#define SAMPLER(I) uniform sampler1D t##I;
#expand SAMPLER _AVG_FRAMES

#define WIN_FUNC window_frame

void main() {
    float r = 0;
    
    /* Disable windowing for two frames (distorts results) */
    #if _AVG_FRAMES == 2
    #define _AVG_WINDOW 0
    #endif

    /* Use 'shallow' windowing for 3 frames to ensure the first & last
       frames have a reasonable amount of weight */
    #if _AVG_FRAMES == 3
    #define WIN_FUNC window_shallow
    #endif
    
    #if _AVG_WINDOW == 0
    #define F(I) r += texelFetch(t##I, int(gl_FragCoord.x), 0).r
    #else
    #define F(I) r += window(I, _AVG_FRAMES - 1) * texelFetch(t##I, int(gl_FragCoord.x), 0).r
    #endif
    #expand F _AVG_FRAMES
    
    fragment.r = r / _AVG_FRAMES;
}
