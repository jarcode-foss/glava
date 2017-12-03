
layout(pixel_center_integer) in vec4 gl_FragCoord;

#request uniform "prev" tex
uniform sampler2D tex;    /* screen texture    */
#request uniform "screen" screen
uniform ivec2     screen; /* screen dimensions */

out vec4 fragment; /* output */

#include "../graph.glsl"

void main() {
    fragment = texture(tex, vec2(gl_FragCoord.x / screen.x, gl_FragCoord.y / screen.y));
    
    vec4
        a0 = texture(tex, vec2((gl_FragCoord.x + 1) / screen.x, (gl_FragCoord.y + 0) / screen.y)),
        a1 = texture(tex, vec2((gl_FragCoord.x + 1) / screen.x, (gl_FragCoord.y + 1) / screen.y)),
        a2 = texture(tex, vec2((gl_FragCoord.x + 0) / screen.x, (gl_FragCoord.y + 1) / screen.y)),
        a3 = texture(tex, vec2((gl_FragCoord.x + 1) / screen.x, (gl_FragCoord.y + 0) / screen.y)),
    
        a4 = texture(tex, vec2((gl_FragCoord.x - 1) / screen.x, (gl_FragCoord.y - 0) / screen.y)),
        a5 = texture(tex, vec2((gl_FragCoord.x - 1) / screen.x, (gl_FragCoord.y - 1) / screen.y)),
        a6 = texture(tex, vec2((gl_FragCoord.x - 0) / screen.x, (gl_FragCoord.y - 1) / screen.y)),
        a7 = texture(tex, vec2((gl_FragCoord.x - 1) / screen.x, (gl_FragCoord.y - 0) / screen.y));

    vec4 avg = (a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7) / 8.0;
    if (avg.a > 0){
        if (fragment.a <= 0 && gl_FragCoord.y != screen.y - 1) {
            /* outline */
            fragment = OUTLINE;
        } else if (avg.a < 1 && gl_FragCoord.y != 0) {
            /* creates a nice 'glint' along the edge of the spectrum */
            fragment.rgb *= avg.a * 2;
        }
    }
}
