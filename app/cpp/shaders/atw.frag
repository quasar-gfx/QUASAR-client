out vec4 FragColor;

in vec2 TexCoords;
flat in float IsLeftEye;

uniform sampler2D videoTexture;

uniform mat4 projectionInverseLeft;
uniform mat4 projectionInverseRight;
uniform mat4 viewInverseRight;
uniform mat4 viewInverseLeft;

uniform mat4 remoteProjectionLeft;
uniform mat4 remoteProjectionRight;
uniform mat4 remoteViewLeft;
uniform mat4 remoteViewRight;

uniform bool atwEnabled;

vec3 ndcToView(mat4 projectionInverse, vec2 ndc, float depth) {
    vec4 ndcPos;
    ndcPos.xy = ndc;
    ndcPos.z = 2.0 * depth - 1.0;
    ndcPos.w = 1.0;

    vec4 viewCoord = projectionInverse * ndcPos;
    viewCoord = viewCoord / viewCoord.w;
    return viewCoord.xyz;
}

vec3 viewToWorld(mat4 viewInverse, vec3 viewCoord) {
    vec4 worldCoord = viewInverse * vec4(viewCoord, 1.0);
    worldCoord = worldCoord / worldCoord.w;
    return worldCoord.xyz;
}

vec3 worldToView(mat4 view, vec3 worldCoord) {
    vec4 viewCoord = view * vec4(worldCoord, 1.0);
    viewCoord = viewCoord / viewCoord.w;
    return viewCoord.xyz;
}

vec2 viewToNDC(mat4 projection, vec3 viewCoord) {
    vec4 ndcCoord = projection * vec4(viewCoord, 1.0);
    ndcCoord = ndcCoord / ndcCoord.w;
    return ndcCoord.xy;
}

vec2 ndcToScreen(vec2 ndc) {
    return (ndc + 1.0) / 2.0;
}

vec2 worldToScreen(mat4 view, mat4 projection, vec3 worldCoord) {
    vec2 ndc = viewToNDC(projection, worldToView(view, worldCoord));
    return ndcToScreen(ndc);
}

void main() {
    if (!atwEnabled) {
        vec2 adjustedTexCoords = TexCoords;
        if (IsLeftEye > 0.5) {
            adjustedTexCoords.x = TexCoords.x / 2.0;
            FragColor = vec4(texture(videoTexture, adjustedTexCoords).rgb, 1.0);
        }
        else {
            adjustedTexCoords.x = TexCoords.x / 2.0 + 0.5;
            FragColor = vec4(texture(videoTexture, adjustedTexCoords).rgb, 1.0);
        }
        return;
    }

    vec2 ndc = TexCoords * 2.0 - 1.0;

    vec3 viewCoord;
    vec3 worldCoord;
    vec2 TexCoordsRemote;
    if (IsLeftEye > 0.5) {
        viewCoord = ndcToView(projectionInverseLeft, ndc, 1.0);
        worldCoord = viewToWorld(viewInverseLeft, viewCoord);
        TexCoordsRemote = worldToScreen(remoteViewLeft, remoteProjectionLeft, worldCoord);
        TexCoordsRemote.x = clamp(TexCoordsRemote.x / 2.0, 0.0, 0.4999);
    }
    else {
        viewCoord = ndcToView(projectionInverseRight, ndc, 1.0);
        worldCoord = viewToWorld(viewInverseRight, viewCoord);
        TexCoordsRemote = worldToScreen(remoteViewRight, remoteProjectionRight, worldCoord);
        TexCoordsRemote.x = clamp(TexCoordsRemote.x / 2.0 + 0.5, 0.5, 0.9999);
    }

    FragColor = vec4(texture(videoTexture, TexCoordsRemote).rgb, 1.0);
}
