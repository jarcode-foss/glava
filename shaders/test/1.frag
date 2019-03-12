/* Request transforms and basic uniforms to assert nothing here breaks */

#include ":util/smooth.glsl"

in vec4 gl_FragCoord;

#request uniform "screen" screen
uniform ivec2 screen;

#request uniform "audio_sz" audio_sz
uniform int audio_sz;

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

void main() {
    float dummy_result0 = smooth_audio(audio_l, audio_sz, gl_FragCoord.x / float(screen.x));
    float dummy_result1 = smooth_audio(audio_r, audio_sz, gl_FragCoord.x / float(screen.x));
    fragment = vec4(1.0, 0, 0, float(1) / float(3));
}
