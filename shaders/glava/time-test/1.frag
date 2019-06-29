#request uniform "time" time
uniform float time;

out vec4 fragment;

void main() { fragment = vec4(sin(time) / 2.0 + 0.5, 0.0, 0.0, 1.0); }