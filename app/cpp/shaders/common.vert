#extension GL_OVR_multiview : enable
layout(num_views = 2) in;

layout(std140, binding = 0) uniform Normals {
    vec4 normals[6];
};
layout(location = 0) in highp vec4 a_Positions;

layout(location = 0) out flat uvec2 o_TexCoord;
layout(location = 1) out highp vec3 o_Normal;
layout(location = 2) out flat vec3 o_Color;

uniform mat4 view[2];
uniform mat4 projection[2];
uniform mat4 model;

uniform vec3 color;

void main() {
    int face = gl_VertexID / 6;
    o_TexCoord = uvec2(face, 0);
    o_Normal = (model * normals[face]).xyz;
    o_Color = color.rgb;

    gl_Position = projection[gl_ViewID_OVR] * view[gl_ViewID_OVR] * model * a_Positions;
}
