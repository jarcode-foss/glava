
layout(pixel_center_integer) in vec4 gl_FragCoord;

#request uniform "screen" screen
uniform ivec2     screen; /* screen dimensions */

#request uniform "audio_l" audio_l
#request transform audio_l "window"
#request transform audio_l "wrange"
uniform sampler1D audio_l;

out vec4 fragment;

#include "settings.glsl"

void main() {
    float os = ((texture(audio_l, gl_FragCoord.x / screen.x).r - 0.5) * AMPLIFY) + 0.5f;
    float s = (os + (screen.y * 0.5f) - 0.5f); /* center to screen coords */
    if (abs(s - gl_FragCoord.y) < clamp(abs(s - (screen.y * 0.5)) * 6, MIN_THICKNESS, MAX_THICKNESS)) {
        fragment = BASE_COLOR + (abs((screen.y * 0.5f) - s) * 0.02);
    } else {
        fragment = vec4(0, 0, 0, 0);
    }
}
