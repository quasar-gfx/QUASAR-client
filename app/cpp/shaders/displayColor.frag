out vec4 FragColor;

in vec2 TexCoords;
flat in float IsLeftEye;

uniform sampler2D videoTexture;

void main() {
    vec2 uv = TexCoords;
    if (IsLeftEye > 0.5) {
        uv.x = uv.x / 2.0;
    }
    else {
        uv.x = uv.x / 2.0 + 0.5;
    }

    FragColor = texture(videoTexture, uv);

    // make checkerboard
    // if (int(uv.x * 10.0) % 2 == int(uv.y * 10.0) % 2) {
    //     if (IsLeftEye > 0.5) {
    //         FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    //     }
    //     else {
    //         FragColor = vec4(0.0, 1.0, 0.0, 1.0);
    //     }
    // }
    // else {
    //     if (IsLeftEye > 0.5) {
    //         FragColor = vec4(0.0, 1.0, 0.0, 1.0);
    //     }
    //     else {
    //         FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    //     }
    // }
}
