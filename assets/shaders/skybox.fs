#version 330

in vec3 direction;
uniform samplerCube environmentMap;
out vec4 finalColor;

void main() {
    finalColor = vec4(texture(environmentMap, direction).rgb, 1.0);
}
