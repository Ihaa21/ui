#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

struct ui_rect
{
    // NOTE: These values are predivided by resolution
    vec2 Center;
    vec2 Radius;
    
    float Z;
    vec4 Color;
};

layout(set = 0, binding = 0) buffer ui_rect_array
{
    ui_rect UiRectArray[];
};

#if RECT_VERTEX

layout(location = 0) in vec3 InPos;

layout(location = 0) out flat uint OutInstanceId;

void main()
{
    ui_rect Rect = UiRectArray[gl_InstanceIndex];

    vec2 UiSpacePos = InPos.xy * Rect.Radius + Rect.Center;
    vec2 NdcPos = 2.0f * UiSpacePos - 1.0f; 
    
    gl_Position = vec4(NdcPos, Rect.Z, 1);
    OutInstanceId = gl_InstanceIndex;
}

#endif

#if RECT_FRAGMENT

layout(location = 0) in flat uint InInstanceId;

layout(location = 0) out vec4 OutColor;

void main()
{
    ui_rect Rect = UiRectArray[InInstanceId];
    OutColor = Rect.Color;
}

#endif
