uniform sampler1D tex;

out vec4 fragment;
in vec4 gl_FragCoord;

/* 1D texture mapping */
void main() {
    fragment.r = texelFetch(tex, int(gl_FragCoord.x), 0).r;
}
