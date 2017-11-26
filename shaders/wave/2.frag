
layout(pixel_center_integer) in vec4 gl_FragCoord;

#request uniform "prev" tex
uniform sampler2D tex;    /* screen texture    */
#request uniform "screen" screen
uniform ivec2     screen; /* screen dimensions */

out vec4 fragment; /* output */

void main() {
    fragment = texture(tex, vec2(gl_FragCoord.x / screen.x, gl_FragCoord.y / screen.y));
    
    float a0 = texture(tex, vec2((gl_FragCoord.x + 1) / screen.x, (gl_FragCoord.y + 0) / screen.y)).a;
    float a1 = texture(tex, vec2((gl_FragCoord.x + 1) / screen.x, (gl_FragCoord.y + 1) / screen.y)).a;
    float a2 = texture(tex, vec2((gl_FragCoord.x + 0) / screen.x, (gl_FragCoord.y + 1) / screen.y)).a;
    float a3 = texture(tex, vec2((gl_FragCoord.x + 1) / screen.x, (gl_FragCoord.y + 0) / screen.y)).a;
    
    float a4 = texture(tex, vec2((gl_FragCoord.x - 1) / screen.x, (gl_FragCoord.y - 0) / screen.y)).a;
    float a5 = texture(tex, vec2((gl_FragCoord.x - 1) / screen.x, (gl_FragCoord.y - 1) / screen.y)).a;
    float a6 = texture(tex, vec2((gl_FragCoord.x - 0) / screen.x, (gl_FragCoord.y - 1) / screen.y)).a;
    float a7 = texture(tex, vec2((gl_FragCoord.x - 1) / screen.x, (gl_FragCoord.y - 0) / screen.y)).a;

    float avg = (a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7) / 8.0;
    fragment *= avg;
}
