/* Pass the initial results to a dummy shader to assert that linking works correctly */

in vec4 gl_FragCoord;

#request uniform "prev" tex
uniform sampler2D tex;    /* screen texture    */

out vec4 fragment; /* output */

void main() {
    fragment = texelFetch(tex, ivec2(gl_FragCoord.x, gl_FragCoord.y), 0);
}
