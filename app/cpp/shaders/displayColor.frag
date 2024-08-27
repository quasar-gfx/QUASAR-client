out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenColor;
uniform sampler2D screenDepth;

// uniform bool doToneMapping = true;
// uniform float exposure = 1.0;

void main() {
    // if (doToneMapping) {
    //     vec3 hdrCol = texture(screenColor, TexCoords).rgb;
    //     vec3 toneMappedResult = vec3(1.0) - exp(-hdrCol * exposure);
    //     FragColor = vec4(toneMappedResult, 1.0);
    // }
    // else {
    //     FragColor = texture(screenColor, TexCoords);
    // }

    if (TexCoords.x > 0.5)
        FragColor = vec4(1,0,0, 1.0);
    else
        FragColor = vec4(0,1,0, 1.0);
}
