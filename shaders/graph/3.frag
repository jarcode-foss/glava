
in vec4 gl_FragCoord;

#request uniform "screen" screen
uniform ivec2 screen; /* screen dimensions */

#request uniform "prev" tex
uniform sampler2D tex;    /* screen texture    */

out vec4 fragment; /* output */

#include "@graph.glsl"
#include ":graph.glsl"

float get_col_height_up(float x, float oy) {
    float y = oy;
    while (y < screen.y) {
        vec4 f = texelFetch(tex, ivec2(x, y), 0);
        if (f.a <= 0) {
            y -= 1;
            break;
        }
        y += 1;
    }

    return y;
}

float get_col_height_down(float x, float oy) {
    float y = oy;
    while (y >= 0) {
        vec4 f = texelFetch(tex, ivec2(x, y), 0);
        if (f.a > 0) {
            break;
        }
        y -= 1;
    }

    return y;
}

void main() {
    fragment = texelFetch(tex, ivec2(gl_FragCoord.x, gl_FragCoord.y), 0);

    #if ANTI_ALIAS > 0

    if (fragment.a <= 0) {
        bool left_done = false;
        float h2;
        float a_fact = 0;

        if (texelFetch(tex, ivec2(gl_FragCoord.x - 1, gl_FragCoord.y), 0).a > 0) {
            float h1 = get_col_height_up(gl_FragCoord.x - 1, gl_FragCoord.y);
            h2 = get_col_height_down(gl_FragCoord.x, gl_FragCoord.y);
            fragment = texelFetch(tex, ivec2(gl_FragCoord.x, h2), 0);

            a_fact = clamp((h1 - gl_FragCoord.y) / abs(h2 - h1), 0.0, 1.0);

            left_done = true;
        }
        if (texelFetch(tex, ivec2(gl_FragCoord.x + 1, gl_FragCoord.y), 0).a > 0) {
            if (!left_done) {
                h2 = get_col_height_down(gl_FragCoord.x, gl_FragCoord.y);
                fragment = texelFetch(tex, ivec2(gl_FragCoord.x, h2), 0);
            }
            float h3 = get_col_height_up(gl_FragCoord.x + 1, gl_FragCoord.y);

            a_fact = max(a_fact, clamp((h3 - gl_FragCoord.y) / abs(h2 - h3), 0.0, 1.0));
        }

        fragment.a *= a_fact;

    }

    #endif
}
