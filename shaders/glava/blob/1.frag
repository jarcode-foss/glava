in vec4 gl_FragCoord;

#request uniform "screen" screen
uniform ivec2 screen;

#request uniform "audio_sz" audio_sz
uniform int audio_sz;

#include ":util/smooth.glsl"
#include ":blob.glsl"

#request uniform "audio_l" audio_l
#request transform audio_l "window"
#request transform audio_l "fft"
#request transform audio_l "gravity"
#request transform audio_l "avg"
uniform sampler1D audio_l;

/* 
* #request uniform "audio_r" audio_r
* #request transform audio_r "window"
* #request transform audio_r "fft"
* #request transform audio_r "gravity"
* #request transform audio_r "avg"
* uniform sampler1D audio_r;
*/

#request uniform "time" time
uniform float time;

out vec4 fragment;

void main() {

#if _USE_ALPHA > 0
#define APPLY_FRAG(f, c) f = vec4(f.rgb * f.a + c.rgb * (1 - clamp(f.a, 0, 1)), max(c.a, f.a))
  fragment = #00000000;
#else
#define APPLY_FRAG(f, c) f = c
#endif

  vec2 v = (gl_FragCoord.xy - (screen * 0.5)) / min(screen.y, screen.x) * 10.0;
  float t = time * 0.8; // * (smooth_audio(audio_l, audio_sz, FREQ) + 1.0) * TIME_MULT;
  float r = smooth_audio(audio_l, audio_sz, FREQ) * MULT;
  for (int i = 0; i < N; i++) {
    float d = (3.14159265 / float(N)) * (float(i) * 5.0);
    r += length(vec2(v.x, v.y)) + 0.01;
    v = vec2(v.x + cos(v.y + cos(r) + d) + cos(t),
             v.y - sin(v.x + cos(r) + d) + sin(t));
  }
  r = (sin(r * 0.1) * 0.5) + 0.5;
  r = pow(r, 128.0);
  float g = pow(max(r - 0.75, 0.0) * 4.0, 2.0);
  float b = pow(max(r - 0.875, 0.0) * 8.0, 4.0);
  APPLY_FRAG(fragment, vec4(r, g, b, r));
}