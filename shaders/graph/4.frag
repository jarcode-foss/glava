
in vec4 gl_FragCoord;

#request uniform "screen" screen
uniform ivec2 screen; /* screen dimensions */

#request uniform "prev" tex
uniform sampler2D tex;    /* screen texture    */

out vec4 fragment; /* output */

#include "@graph.glsl"
#include ":graph.glsl"

void main() {
    float newy = gl_FragCoord.y;
    #if INVERT > 0
    newy = float(screen.y + 1) - newy;
    #endif

    fragment = texelFetch(tex, ivec2(gl_FragCoord.x, newy), 0);
}
