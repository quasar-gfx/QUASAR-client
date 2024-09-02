layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoords;

#ifdef ANDROID
layout(num_views = 2) in;
#endif

out vec2 TexCoords;
flat out float IsLeftEye;

void main() {
    TexCoords = aTexCoords;
    IsLeftEye = (float(gl_ViewID_OVR) < 0.5) ? 1.0 : 0.0;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
