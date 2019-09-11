uniform sampler1D tex;
uniform float diff;

out vec4 fragment;
in vec4 gl_FragCoord;

void main() {
    fragment.r = texelFetch(tex, int(gl_FragCoord.x), 0).r - diff;
}
